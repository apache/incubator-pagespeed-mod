#!/bin/bash
#
# Runs system tests for system/ and automatic/.
#
# See automatic/system_test_helpers.sh for usage.
#

# Default to not running the controller, unless specified to
# do so via an environment variable.
RUN_CONTROLLER_TEST=${RUN_CONTROLLER_TEST:-off}

FIRST_RUN=${FIRST_RUN:-false}

# To fetch from the secondary test root, we must set
# http_proxy=${SECONDARY_HOSTNAME} during fetches.
SECONDARY_ROOT="http://secondary.example.com"
SECONDARY_TEST_ROOT="$SECONDARY_ROOT/mod_pagespeed_test"

# Run the automatic/ system tests.
#
# We need to know the directory this file is located in.  Unfortunately,
# if we're 'source'd from a script in a different directory $(dirname $0) gives
# us the directory that *that* script is located in
this_dir=$(dirname "${BASH_SOURCE[0]}")
source "$this_dir/../automatic/system_test.sh" || exit 1

# TODO(jefftk): move all tests from apache/system_test.sh to here except the
# ones that actually are Apache-specific.

# Define a mechanism to start a test before the cache-flush and finish it
# after the cache-flush.  This mechanism is preferable to flushing cache
# within a test as that requires waiting 5 seconds for the poll, so we'd
# like to limit the number of cache flushes and exploit it on behalf of
# multiple tests.

# Variable holding a space-separated lists of bash functions to run after
# flushing cache.
post_cache_flush_test=""

# Adds a new function to run after cache flush.
function on_cache_flush() {
  post_cache_flush_test+=" $1"
}

# Called after cache-flush to run all the functions specified to
# on_cache_flush.
function run_post_cache_flush() {
  for test in $post_cache_flush_test; do
    $test
  done
}

rm -rf $OUTDIR
mkdir -p $OUTDIR

# In Apache users can disable merging the global config into the vhost config.
# If we're running that way then apache_system_test will have set
# NO_VHOST_MERGE to "on".
NO_VHOST_MERGE="${NO_VHOST_MERGE:-off}"

SUDO=${SUDO:-}

start_test Check for correct default pagespeed header format.
# This will be X-Page-Speed in nginx and X-ModPagespeed in apache.  Accept both.
OUT=$($WGET_DUMP $EXAMPLE_ROOT/combine_css.html)
check_from "$OUT" egrep -q \
  '^X-(Mod-Pagespeed|Page-Speed): [0-9]+[.][0-9]+[.][0-9]+[.][0-9]+-[0-9]+'

start_test pagespeed is defaulting to more than PassThrough
if [ ! -z "${APACHE_DOC_ROOT-}" ]; then
  # Note: in Apache this test relies on lack of .htaccess in mod_pagespeed_test.
  check [ ! -f $APACHE_DOC_ROOT/mod_pagespeed_test/.htaccess ]
fi
fetch_until $TEST_ROOT/bot_test.html 'fgrep -c .pagespeed.' 2

start_test ipro resources have etag and not last-modified
URL="$EXAMPLE_ROOT/images/Puzzle.jpg?a=$RANDOM"
# Fetch it a few times until IPRO is done and has given it an ipro ("aj") etag.
fetch_until -save "$URL" 'grep -c E[Tt]ag:.W/.PSA-aj.' 1 --save-headers
# Verify that it doesn't have a Last-Modified header.
check [ $(grep -c "^Last-Modified:" $FETCH_FILE) = 0 ]
# Extract the Etag and verify we get "not modified" when we send the it.
ETAG=$(grep -a -o 'W/"PSA-aj[^"]*"' $FETCH_FILE)
check_from "$ETAG" fgrep PSA
echo $CURL -sS -D- -o/dev/null -H "If-None-Match: $ETAG" $URL
OUT=$($CURL -sS -D- -o/dev/null -H "If-None-Match: $ETAG" $URL)
check_from "$OUT" fgrep "HTTP/1.1 304"
check_not_from "$OUT" fgrep "Content-Length"
# Verify we don't get a 304 with a different Etag.
BAD_ETAG=$(echo "$ETAG" | sed s/PSA-aj/PSA-ic/)
echo $CURL -sS -D- -o/dev/null -H "If-None-Match: $BAD_ETAG" $URL
OUT=$($CURL -sS -D- -o/dev/null -H "If-None-Match: $BAD_ETAG" $URL)
check_not_from "$OUT" fgrep "HTTP/1.1 304"
check_from "$OUT" fgrep "HTTP/1.1 200"
check_from "$OUT" fgrep "Content-Length"

start_test aris disables js inlining for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/"
URL+="?PageSpeedFilters=inline_javascript"
fetch_until $URL 'grep -c src=' 1

start_test aris disables js inlining only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html"
URL+="?PageSpeedFilters=inline_javascript"
fetch_until $URL 'grep -c src=' 0

test_filter rewrite_javascript minifies JavaScript and saves bytes.
start_test aris disables js cache extention for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/"
URL+="?PageSpeedFilters=rewrite_javascript"

# first check something that should get rewritten to know we're done with
# rewriting
fetch_until -save $URL 'grep -c "src=\"../normal.js\""' 0
check [ $(grep -c "src=\"../introspection.js\"" $FETCH_FILE) = 1 ]

start_test aris disables js cache extension only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html"
URL+="?PageSpeedFilters=rewrite_javascript"
fetch_until -save $URL 'grep -c src=\"normal.js\"' 0
check [ $(grep -c src=\"introspection.js\" $FETCH_FILE) = 0 ]

# Check that no filter changes urls for introspective javascript if
# avoid_renaming_introspective_javascript is on
start_test aris disables url modification for introspective js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/"
URL+="?PageSpeedFilters=testing,core"
# first check something that should get rewritten to know we're done with
# rewriting
fetch_until -save $URL 'grep -c src=\"../normal.js\"' 0
check [ $(grep -c src=\"../introspection.js\" $FETCH_FILE) = 1 ]

start_test aris disables url modification only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html"
URL+="?PageSpeedFilters=testing,core"
fetch_until -save $URL 'grep -c src=\"normal.js\"' 0
check [ $(grep -c src=\"introspection.js\" $FETCH_FILE) = 0 ]

start_test can combine css with authorized ids only
URL="$TEST_ROOT/combine_css_with_ids.html?PageSpeedFilters=combine_css"
# Test big.css and bold.css are combined, but not yellow.css or blue.css.
fetch_until -save "$URL" 'fgrep -c styles/big.css+bold.css.pagespeed.cc' 1
check_from "$(cat "$FETCH_FILE")" fgrep -q '/styles/yellow.css" id='
check_from "$(cat "$FETCH_FILE")" fgrep -q '/styles/blue.css" id='

start_test HTML add_instrumentation CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.html\
?PageSpeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
# In some servers PageSpeed runs before response headers are finalized, which
# means it has to  assume the page is xhtml because the 'Content-Type' header
# might just not have been set yet.  In others it runs after, and so it can
# trust what it sees in the headers.  See RewriteDriver::MimeTypeXhtmlStatus().
if $HEADERS_FINALIZED; then
  check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 0 ]
else
  check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]
fi

start_test XHTML add_instrumentation also lacks '&amp;' and contains CDATA
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.xhtml\
?PageSpeedFilters=add_instrumentation
check [ $(grep -c "\&amp;" $WGET_OUTPUT) = 0 ]
check [ $(grep -c '//<\!\[CDATA\[' $WGET_OUTPUT) = 1 ]

start_test cache_partial_html enabled has no effect
$WGET -O $WGET_OUTPUT $TEST_ROOT/add_instrumentation.html\
?PageSpeedFilters=cache_partial_html
check [ $(grep -c '<html>' $WGET_OUTPUT) = 1 ]
check [ $(grep -c '<body>' $WGET_OUTPUT) = 1 ]
check [ $(grep -c 'pagespeed.panelLoader' $WGET_OUTPUT) = 0 ]

start_test flush_subresources rewriter is not applied
URL="$TEST_ROOT/flush_subresources.html?\
PageSpeedFilters=flush_subresources,extend_cache_css,\
extend_cache_scripts"
# Fetch once with X-PSA-Blocking-Rewrite so that the resources get rewritten and
# property cache is updated with them.
wget -O - --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $TESTTMP/flush
# Fetch again. The property cache has the subresources this time but
# flush_subresources rewriter is not applied. This is a negative test case
# because this rewriter does not exist in pagespeed yet.
check [ `wget -O - $URL | grep -o 'link rel="subresource"' | wc -l` = 0 ]
rm -f $TESTTMP/flush

start_test Respect custom options on resources.
IMG_NON_CUSTOM="$EXAMPLE_ROOT/images/xPuzzle.jpg.pagespeed.ic.fakehash.jpg"
IMG_CUSTOM="$TEST_ROOT/custom_options/xPuzzle.jpg.pagespeed.ic.fakehash.jpg"

# Identical images, but in the config for the custom_options directory we
# additionally disable core-filter convert_jpeg_to_progressive which gives a
# larger file.
fetch_until $IMG_NON_CUSTOM 'wc -c' 98276 "" -le
fetch_until $IMG_CUSTOM 'wc -c' 102902 "" -le

start_test LoadFromFile
URL=$TEST_ROOT/load_from_file/index.html?PageSpeedFilters=inline_css
fetch_until $URL 'grep -c blue' 1

# The "httponly" directory is disallowed.
fetch_until $URL 'fgrep -c web.httponly.example.css' 1

# Loading .ssp.css files from file is disallowed.
fetch_until $URL 'fgrep -c web.example.ssp.css' 1

# There's an exception "allow" rule for "exception.ssp.css" so it can be loaded
# directly from the filesystem.
fetch_until $URL 'fgrep -c file.exception.ssp.css' 1

start_test LoadFromFileMatch
URL=$TEST_ROOT/load_from_file_match/index.html?PageSpeedFilters=inline_css
fetch_until $URL 'grep -c blue' 1

start_test Make sure nostore on a subdirectory is retained
URL=$TEST_ROOT/nostore/nostore.html
HTML_HEADERS=$($WGET_DUMP $URL)
check_from "$HTML_HEADERS" egrep -q \
  'Cache-Control: max-age=0, no-cache, no-store'

start_test Custom headers remain on HTML, but cache should be disabled.
URL=$TEST_ROOT/rewrite_compressed_js.html
echo $WGET_DUMP $URL
HTML_HEADERS=$($WGET_DUMP $URL)
check_from "$HTML_HEADERS" egrep -q "X-Extra-Header: 1"
# The extra header should only be added once, not twice.
check_not_from "$HTML_HEADERS" egrep -q "X-Extra-Header: 1, 1"
check_from "$HTML_HEADERS" egrep -q 'Cache-Control: max-age=0, no-cache'

start_test Custom headers remain on resources, but cache should be 1 year.
URL="$TEST_ROOT/compressed/hello_js.custom_ext.pagespeed.ce.HdziXmtLIV.txt"
echo $WGET_DUMP $URL
RESOURCE_HEADERS=$($WGET_DUMP $URL)
check_from "$RESOURCE_HEADERS"  egrep -q 'X-Extra-Header: 1'
# The extra header should only be added once, not twice.
check_not_from "$RESOURCE_HEADERS"  egrep -q 'X-Extra-Header: 1, 1'
check [ "$(echo "$RESOURCE_HEADERS" | grep -c '^X-Extra-Header: 1')" = 1 ]
check_from "$RESOURCE_HEADERS"  egrep -q 'Cache-Control: max-age=31536000'

start_test ModifyCachingHeaders
URL=$TEST_ROOT/retain_cache_control/index.html
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q "Cache-Control: private, max-age=3000"
check_from "$OUT" grep -q "Last-Modified:"

start_test ModifyCachingHeaders with DownstreamCaching enabled.
URL=$TEST_ROOT/retain_cache_control_with_downstream_caching/index.html
OUT=$($WGET_DUMP -S $URL)
check_not_from "$OUT" grep -q "Last-Modified:"
check_from "$OUT" grep -q "Cache-Control: private, max-age=3000"

test_filter combine_javascript combines 2 JS files into 1.
start_test combine_javascript with long URL still works
URL=$TEST_ROOT/combine_js_very_many.html?PageSpeedFilters=combine_javascript
fetch_until $URL 'grep -c src=' 4

start_test aris disables js combining for introspective js and only i-js
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__on/"
URL+="?PageSpeedFilters=combine_javascript"
fetch_until $URL 'grep -c src=' 2

start_test aris disables js combining only when enabled
URL="$TEST_ROOT/avoid_renaming_introspective_javascript__off.html"
URL+="?PageSpeedFilters=combine_javascript"
fetch_until $URL 'grep -c src=' 1

start_test MapProxyDomain
# depends on MapProxyDomain in the config file
LEAF="proxy_external_resource.html?PageSpeedFilters=-inline_images"
URL="$EXAMPLE_ROOT/$LEAF"
echo Rewrite HTML with reference to a proxyable image.
fetch_until -save -recursive $URL 'grep -c 1.gif.pagespeed' 1 --save-headers
PAGESPEED_GIF=$(grep -o '/*1.gif.pagespeed[^"]*' $WGET_DIR/$LEAF)
check_from "$PAGESPEED_GIF" grep "gif$"

echo "If the next line fails, look in $WGET_DIR/wget_output.txt and you should"
echo "see a 404.  This represents a failed attempt to download the proxied gif."
# TODO(jefftk): debug why this test sometimes fails with the native fetcher in
# ngx_pagespeed.  https://github.com/pagespeed/ngx_pagespeed/issues/774
check test -f "$WGET_DIR$PAGESPEED_GIF"

# To make sure that we can reconstruct the proxied content by going back
# to the origin, we must avoid hitting the output cache.
# Note that cache-flushing does not affect the cache of rewritten resources;
# only input-resources and metadata.  To avoid hitting that cache and force
# us to rewrite the resource from origin, we grab this resource from a
# virtual host attached to a different cache.
if [ "$SECONDARY_HOSTNAME" != "" ]; then
  SECONDARY_HOST="$SECONDARY_ROOT/gstatic_images"
  PROXIED_IMAGE="$SECONDARY_HOST$PAGESPEED_GIF"
  start_test $PROXIED_IMAGE expecting one year cache.

  # With the proper hash, we'll get a long cache lifetime.
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save $PROXIED_IMAGE \
      "grep -c max-age=31536000" 1 --save-headers

  # With the wrong hash, we'll get a short cache lifetime (and also no output
  # cache hit.
  WRONG_HASH="0"
  PROXIED_IMAGE="$SECONDARY_HOST/1.gif.pagespeed.ce.$WRONG_HASH.jpg"
  start_test Fetching $PROXIED_IMAGE expecting short private cache.
  http_proxy=$SECONDARY_HOSTNAME fetch_until $PROXIED_IMAGE \
      "grep -c max-age=300,private" 1 --save-headers

  # Test fetching a pagespeed URL via a reverse proxy, with pagespeed loaded,
  # but disabled for the proxied domain. As reported in Issue 582 this used to
  # fail with a 403 (Forbidden).
  start_test Reverse proxy a pagespeed URL.

  PROXY_PATH="http://$PAGESPEED_TEST_HOST/mod_pagespeed_example/styles"
  ORIGINAL="${PROXY_PATH}/yellow.css"
  FILTERED="${PROXY_PATH}/A.yellow.css.pagespeed.cf.KM5K8SbHQL.css"

  # We should be able to fetch the original ...
  echo  http_proxy=$SECONDARY_HOSTNAME $WGET --save-headers -O - $ORIGINAL
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET --save-headers -O - $ORIGINAL 2>&1)
  check_200_http_response "$OUT"
  # ... AND the rewritten version.
  echo  http_proxy=$SECONDARY_HOSTNAME $WGET --save-headers -O - $FILTERED
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET --save-headers -O - $FILTERED 2>&1)
  check_200_http_response "$OUT"
fi

start_test proxying from external domain should optimize images in-place.
# Keep fetching this until it's headers include the string "PSA-aj" which
# means rewriting has finished.
URL="$PRIMARY_SERVER/modpagespeed_http/Puzzle.jpg"
fetch_until -save $URL "grep -c PSA-aj" 1 "--save-headers"

# We should see the origin etag in the wget output due to -save.  Note that
# the cache-control will start at 5 minutes -- the default on modpagespeed.com,
# and descend as time expires from when we strobed the image.  However, we
# provide a non-trivial etag with the content hash, but we'll just match the
# common prefix.
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    fgrep -qi 'Etag: W/"PSA-aj-'

# Ideally this response should not have a 'chunked' encoding, because
# once we were able to optimize it, we know its length.
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" fgrep -q 'Content-Length:'
check_not_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    fgrep -q 'Transfer-Encoding: chunked'

# Now add set jpeg compression to 75 and we expect 73238, but will test for 90k.
# Note that wc -c will include the headers.
start_test Proxying image from another domain, customizing image compression.
URL+="?PageSpeedJpegRecompressionQuality=75"
fetch_until -save $URL "wc -c" 90000 "--save-headers" "-lt"
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    fgrep -qi 'Etag: W/"PSA-aj-'

echo Ensure that rewritten images strip cookies present at origin
check_not_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" fgrep -qi 'Set-Cookie'
$WGET -O $FETCH_UNTIL_OUTFILE --save-headers \
  http://$PAGESPEED_TEST_HOST/do_not_modify/Puzzle.jpg
ORIGINAL_HEADERS=$(extract_headers $FETCH_UNTIL_OUTFILE)
check_from "$ORIGINAL_HEADERS" fgrep -q -i 'Set-Cookie'

start_test proxying HTML from external domain should not work
URL="$PRIMARY_SERVER/modpagespeed_http/evil.html"
OUT=$(check_error_code 8 $WGET_DUMP $URL)
check_not_from "$OUT" fgrep -q 'Set-Cookie:'

start_test Fetching the HTML directly from the origin is fine including cookie.
URL="http://$PAGESPEED_TEST_HOST/do_not_modify/evil.html"
OUT=$($WGET_DUMP $URL)
check_from "$OUT" fgrep -q -i 'Set-Cookie: test-cookie'

start_test Ipro transcode to webp from MapProxyDomain
URL="$PRIMARY_SERVER/modpagespeed_http/Puzzle.jpg"
URL+="?PageSpeedFilters=+in_place_optimize_for_browser"
WGET_ARGS="--user-agent webp --header Accept:image/webp"
fetch_until "$URL" "grep -c image/webp" 1 --save-headers
URL=""

# Tests that we get instant ipro rewrites with LoadFromFile and
# InPlaceWaitForOptimized get us first-pass rewrites.
start_test instant ipro with InPlaceWaitForOptimized and LoadFromFile
echo $WGET_DUMP $TEST_ROOT/ipro/instant/wait/purple.css
OUT=$($WGET_DUMP $TEST_ROOT/ipro/instant/wait/purple.css)
check_from "$OUT" fgrep -q 'body{background:#9370db}'

start_test instant ipro with ModPagespeedInPlaceRewriteDeadline and LoadFromFile
echo $WGET_DUMP $TEST_ROOT/ipro/instant/deadline/purple.css
OUT=$($WGET_DUMP $TEST_ROOT/ipro/instant/deadline/purple.css)
check_from "$OUT" fgrep -q 'body{background:#9370db}'

if [ "$RUN_CONTROLLER_TEST" = "on" ]; then
  start_test IPRO requests are routed through the controller API

  STATS=$OUTDIR/controller_stats
  $WGET_DUMP $GLOBAL_STATISTICS_URL > $STATS.0

  OUT=$($WGET_DUMP $TEST_ROOT/ipro/instant/wait/purple.css?random=$RANDOM)
  check_from "$OUT" fgrep -q 'body{background:#9370db}'

  $WGET_DUMP $GLOBAL_STATISTICS_URL > $STATS.1
  check_stat $STATS.0 $STATS.1 named-lock-rewrite-scheduler-granted 0
  check_stat $STATS.0 $STATS.1 popularity-contest-num-rewrites-succeeded 1
fi

start_test json keeps its content type
URL="$TEST_ROOT/example.json"
OUT=$($WGET_DUMP "$URL?PageSpeed=off")
# Verify that it's application/json without PageSpeed touching it.
check_from "$OUT" grep '^Content-Type: application/json'
OUT=$($WGET_DUMP "$URL")
# Verify that it's application/json on the first PageSpeed load.
check_from "$OUT" grep '^Content-Type: application/json'
# Fetch it repeatedly until it's been IPRO-optimized.  This grep command is kind
# of awkward, because fetch_until doesn't do quoting well.
WGET_ARGS="--save-headers" fetch_until -save "$URL" \
  "grep -c .title.:.example.json" 1
OUT=$(cat $FETCH_UNTIL_OUTFILE)
# Make sure we didn't change the content type to application/javascript.
check_from "$OUT" grep '^Content-Type: application/json'

start_test ShardDomain directive in per-directory config
fetch_until -save $TEST_ROOT/shard/shard.html 'fgrep -c .pagespeed.ce' 4
check [ $(grep -ce href=\"http://shard1 $FETCH_FILE) = 2 ];
check [ $(grep -ce href=\"http://shard2 $FETCH_FILE) = 2 ];

start_test server-side includes
fetch_until -save $TEST_ROOT/ssi/ssi.shtml?PageSpeedFilters=combine_css \
    'fgrep -c .pagespeed.' 1
check [ $(grep -ce $combine_css_filename $FETCH_FILE) = 1 ];

# Test our handling of headers when a FLUSH event occurs, using PHP.
# Tests that require PHP can be disabled by setting DISABLE_PHP_TESTS to
# non-empty, to cater to admins who don't want PHP installed.
if [ -z "${DISABLE_PHP_TESTS:-}" ]; then
  # Fetch the first file so we can check if PHP is enabled.
  start_test PHP is enabled.
  FILE=php_withoutflush.php
  URL=$TEST_ROOT/$FILE
  FETCHED=$WGET_DIR/$FILE
  # wget returns non-zero on 4XX and 5XX, both of which can occur with a
  # mis-configured PHP setup. We need to mask that because of set -e.
  $WGET_DUMP $URL > $FETCHED || true
  if ! grep -q '^HTTP/1.1 200' $FETCHED || grep -q '<?php' $FETCHED; then
    echo "*** PHP is not installed/working. If you'd like to enable this"
  echo "*** test please run: sudo apt-get install php5-common php5"
  echo
  echo "If php is already installed, run it with:"
  echo "    php-cgi -b 127.0.0.1:9000"
    echo
    echo "To disable php tests, set DISABLE_PHP_TESTS to non-empty"
    exit 1
  fi

  # Now we know PHP is working, proceed with the actual testing.
  start_test Headers are not destroyed by a flush event.
  check [ $(grep -c '^X-\(Mod-Pagespeed\|Page-Speed\):' $FETCHED) = 1 ]
  check [ $(grep -c '^X-My-PHP-Header: without_flush' $FETCHED) = 1 ]

  # mod_pagespeed doesn't clear the content length header if there aren't any
  # flushes, but ngx_pagespeed does.  It's possible that ngx_pagespeed should
  # also avoid clearing the content length, but it doesn't and I don't think
  # it's important, so don't check for content-length.
  # check [ $(grep -c '^Content-Length: [0-9]'          $FETCHED) = 1 ]

  FILE=php_withflush.php
  URL=$TEST_ROOT/$FILE
  FETCHED=$WGET_DIR/$FILE
  $WGET_DUMP $URL > $FETCHED
  check [ $(grep -c '^X-\(Mod-Pagespeed\|Page-Speed\):' $FETCHED) = 1 ]
  check [ $(grep -c '^X-My-PHP-Header: with_flush'    $FETCHED) = 1 ]
  # 2.2 prefork returns no content length while 2.2 worker returns a real
  # content length. IDK why but skip this test because of that.
  # check [ $(grep -c '^Content-Length: [0-9]'          $FETCHED) = 1 ]
fi

if [ $statistics_enabled = "1" ]; then
  start_test 404s are served and properly recorded.
  echo $STATISTICS_URL
  NUM_404=$(scrape_stat resource_404_count)
  echo "Initial 404s: $NUM_404"
  WGET_ERROR=$(check_not $WGET -O /dev/null $BAD_RESOURCE_URL 2>&1)
  check_from "$WGET_ERROR" fgrep -q "404 Not Found"

  # Check that the stat got bumped.
  NUM_404_FINAL=$(scrape_stat resource_404_count)
  echo "Final 404s: $NUM_404_FINAL"
  check [ $(expr $NUM_404_FINAL - $NUM_404) -eq 1 ]

  # Check that the stat doesn't get bumped on non-404s.
  URL="$PRIMARY_SERVER/mod_pagespeed_example/styles/"
  URL+="W.rewrite_css_images.css.pagespeed.cf.Hash.css"
  OUT=$(wget -O - -q $URL)
  check_from "$OUT" grep background-image
  NUM_404_REALLY_FINAL=$(scrape_stat resource_404_count)
  check [ $NUM_404_FINAL -eq $NUM_404_REALLY_FINAL ]

  # This test only makes sense if you're running tests against localhost.
  if echo "$HOSTNAME" | grep "^localhost:"; then
    if which ifconfig >/dev/null; then
      start_test Non-local access to statistics fails.
      NON_LOCAL_IP=$( \
        ifconfig | egrep -o 'inet addr:[0-9]+.[0-9]+.[0-9]+.[0-9]+' | \
        awk -F: '{print $2}' | grep -v ^127 | head -n 1)

      # Make sure pagespeed is listening on NON_LOCAL_IP.
      URL="http://$NON_LOCAL_IP:$(echo $HOSTNAME | sed s/^localhost://)/"
      URL+="mod_pagespeed_example/styles/"
      URL+="W.rewrite_css_images.css.pagespeed.cf.Hash.css"
      OUT=$($CURL -Ssi $URL)
      check_from "$OUT" grep background-image

      # Make sure we can't load statistics from NON_LOCAL_IP.
      ALT_STAT_URL=$(echo $STATISTICS_URL | sed s#localhost#$NON_LOCAL_IP#)

      echo "wget $ALT_STAT_URL >& $TESTTMP/alt_stat_url"
      check_error_code 8 wget $ALT_STAT_URL >& "$TESTTMP/alt_stat_url"
      rm -f "$TESTTMP/alt_stat_url"

      ALT_CE_URL="$ALT_STAT_URL.pagespeed.ce.8CfGBvwDhH.css"
      check_error_code 8 wget -O - $ALT_CE_URL  >& "$TESTTMP/alt_ce_url"
      check_error_code 8 wget -O - --header="Host: $HOSTNAME" $ALT_CE_URL \
        >& "$TESTTMP/alt_ce_url"
      rm -f "$TESTTMP/alt_ce_url"
    fi
  fi

  # Even though we don't have a cookie, we will conservatively avoid
  # optimizing resources with Vary:Cookie set on the response, so we
  # will not get the instant response, of "body{background:#9370db}":
  # 24 bytes, but will get the full original text:
  #     "body {\n    background: MediumPurple;\n}\n"
  # This will happen whether or not we send a cookie.
  #
  # Testing this requires proving we'll never optimize something, which
  # can't be distinguished from the not-yet-optimized case, except by the
  # ipro_not_rewritable stat, so we loop by scraping that stat and seeing
  # when it changes.

  # Executes commands until ipro_no_rewrite_count changes.  The
  # command-line options are all passed to WGET_DUMP.  Leaves command
  # wget output in $IPRO_OUTPUT.
  function ipro_expect_no_rewrite() {
    ipro_no_rewrite_count_start=$(scrape_stat ipro_not_rewritable)
    ipro_no_rewrite_count=$ipro_no_rewrite_count_start
    iters=0
    while [ $ipro_no_rewrite_count -eq $ipro_no_rewrite_count_start ]; do
      if [ $iters -ne 0 ]; then
        sleep 0.1
        if [ $iters -gt 100 ]; then
          echo TIMEOUT
          exit 1
        fi
      fi
      IPRO_OUTPUT=$($WGET_DUMP "$@")
      ipro_no_rewrite_count=$(scrape_stat ipro_not_rewritable)
      iters=$((iters + 1))
    done
  }

  start_test ipro with vary:cookie with no cookie set
  ipro_expect_no_rewrite $TEST_ROOT/ipro/cookie/vary_cookie.css
  check_from "$IPRO_OUTPUT" fgrep -q '    background: MediumPurple;'
  check_from "$IPRO_OUTPUT" egrep -q 'Vary: (Accept-Encoding,)?Cookie'

  start_test ipro with vary:cookie with cookie set
  ipro_expect_no_rewrite $TEST_ROOT/ipro/cookie/vary_cookie.css \
    --header=Cookie:cookie-data
  check_from "$IPRO_OUTPUT" fgrep -q '    background: MediumPurple;'
  check_from "$IPRO_OUTPUT" egrep -q 'Vary: (Accept-Encoding,)?Cookie'

  start_test ipro with vary:cookie2 with no cookie2 set
  ipro_expect_no_rewrite $TEST_ROOT/ipro/cookie2/vary_cookie2.css
  check_from "$IPRO_OUTPUT" fgrep -q '    background: MediumPurple;'
  check_from "$IPRO_OUTPUT" egrep -q 'Vary: (Accept-Encoding,)?Cookie2'

  start_test ipro with vary:cookie2 with cookie2 set
  ipro_expect_no_rewrite $TEST_ROOT/ipro/cookie2/vary_cookie2.css \
    --header=Cookie2:cookie2-data
  check_from "$IPRO_OUTPUT" fgrep -q '    background: MediumPurple;'
  check_from "$IPRO_OUTPUT" egrep -q 'Vary: (Accept-Encoding,)?Cookie2'

  start_test authorized resources do not get cached and optimized.
  URL="$TEST_ROOT/auth/medium_purple.css"
  AUTH="Authorization:Basic dXNlcjE6cGFzc3dvcmQ="
  not_cacheable_start=$(scrape_stat ipro_recorder_not_cacheable)
  echo $WGET_DUMP --header="$AUTH" "$URL"
  OUT=$($WGET_DUMP --header="$AUTH" "$URL")
  check_from "$OUT" fgrep -q 'background: MediumPurple;'
  not_cacheable=$(scrape_stat ipro_recorder_not_cacheable)
  check [ $not_cacheable = $((not_cacheable_start + 1)) ]
  URL=""
  AUTH=""

  # Ideally the system should only rewrite an image once when when it gets
  # a burst of requests.  A bug was fixed where we were not obeying a
  # failed lock and were rewriting it potentially many times.  It still
  # happens fairly often that we rewrite the image twice.  I am not sure
  # why that is, except to observe that our locks are 'best effort'.
  start_test A burst of image requests should yield only one two rewrites.
  URL="$EXAMPLE_ROOT/images/Puzzle.jpg?a=$RANDOM"
  start_image_rewrites=$(scrape_stat image_rewrites)
  echo Running burst of 20x: \"wget -q -O - $URL '|' wc -c\"
  for ((i = 0; i < 20; ++i)); do
    echo -n $(wget -q -O - $URL | wc -c) ""
  done
  echo "... done"
  sleep 1
  num_image_rewrites=$(($(scrape_stat image_rewrites) - start_image_rewrites))
  check [ $num_image_rewrites = 1 -o $num_image_rewrites = 2 ]
  URL=""
fi

# The prioritize_critical_css test is split into two functions so
# nginx_system_test.sh can verify that beacon data is preserved across restarts
# via shm-cache checkpointing.  Specifically, the nginx system test first does a
# run of test_prioritize_critical_css, restarts nginx, and then runs
# test_prioritize_critical_css_final.  Because beacon responses are saved in the
# metadata cache this can only pass if the metadata cache is being persisted
# across restarts.
#
# That means this test is run twice when testing, both here and then again later
# on either side of a restart, but it's pretty fast so that's not a problem.
function test_prioritize_critical_css() {
  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    # Test critical CSS beacon injection, beacon return, and computation.  This
    # requires UseBeaconResultsInFilters() to be true in rewrite_driver_factory.
    # NOTE: must occur after cache flush, which is why it's in this embedded
    # block.  The flush removes pre-existing beacon results from the pcache.
    test_filter prioritize_critical_css
    fetch_until -save $URL 'fgrep -c pagespeed.criticalCssBeaconInit' 1
    check [ $(fgrep -o ".very_large_class_name_" $FETCH_FILE | wc -l) -eq 36 ]
    CALL_PAT=".*criticalCssBeaconInit("
    SKIP_ARG="[^,]*,"
    CAPTURE_ARG="'\([^']*\)'.*"
    BEACON_PATH=$(sed -n "s/${CALL_PAT}${CAPTURE_ARG}/\1/p" $FETCH_FILE)
    ESCAPED_URL=$(sed -n \
      "s/${CALL_PAT}${SKIP_ARG}${CAPTURE_ARG}/\1/p" $FETCH_FILE)
    OPTIONS_HASH=$(sed -n \
      "s/${CALL_PAT}${SKIP_ARG}${SKIP_ARG}${CAPTURE_ARG}/\1/p" $FETCH_FILE)
    NONCE=$(sed -n \
      "s/${CALL_PAT}${SKIP_ARG}${SKIP_ARG}${SKIP_ARG}${CAPTURE_ARG}/\1/p" \
      $FETCH_FILE)
    BEACON_URL="http://${HOSTNAME}${BEACON_PATH}?url=${ESCAPED_URL}"
    BEACON_DATA="oh=${OPTIONS_HASH}&n=${NONCE}&cs=.big,.blue,.bold,.foo"

    OUT=$($CURL -sSi -d "$BEACON_DATA" "$BEACON_URL")
    check_from "$OUT" grep '^HTTP/1.1 204'

    test_prioritize_critical_css_final
  fi
}

function test_prioritize_critical_css_final() {
  if [ "$SECONDARY_HOSTNAME" != "" ]; then
    # Now make sure we see the correct critical css rules.
    fetch_until $URL \
      'grep -c <style>[.]blue{[^}]*}</style>' 1
    fetch_until $URL \
      'grep -c <style>[.]big{[^}]*}</style>' 1
    fetch_until $URL \
      'grep -c <style>[.]blue{[^}]*}[.]bold{[^}]*}</style>' 1
    fetch_until -save $URL \
      'grep -c <style>[.]foo{[^}]*}</style>' 1
    # The last one should also have the other 3, too.
    check [ `grep -c '<style>[.]blue{[^}]*}</style>' $FETCH_UNTIL_OUTFILE` = 1 ]
    check [ `grep -c '<style>[.]big{[^}]*}</style>' $FETCH_UNTIL_OUTFILE` = 1 ]
    check [ `grep -c '<style>[.]blue{[^}]*}[.]bold{[^}]*}</style>' \
      $FETCH_UNTIL_OUTFILE` = 1 ]
  fi
}

start_test prioritize critical css
test_prioritize_critical_css

if [ "$SECONDARY_HOSTNAME" != "" ]; then
  start_test query params dont turn on core filters
  # See https://github.com/pagespeed/ngx_pagespeed/issues/1190
  URL="debug-filters.example.com/mod_pagespeed_example/"
  URL+="rewrite_javascript.html?PageSpeedFilters=-rewrite_css"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)
  FILTERS=$(extract_filters_from_debug_html "$OUT")
  check_from "$FILTERS" grep -q "^db.*Debug$"
  check_from "$FILTERS" grep -q "^hw.*Flushes html$"
  check_not_from "$FILTERS" grep -q "^jm.*Rewrite External Javascript$"
  check_not_from "$FILTERS" grep -q "^jj.*Rewrite Inline Javascript$"

  start_test OptimizeForBandwidth
  # We use blocking-rewrite tests because we want to make sure we don't
  # get rewritten URLs when we don't want them.
  function test_optimize_for_bandwidth() {
    SECONDARY_HOST="optimizeforbandwidth.example.com"
    OUT=$(http_proxy=$SECONDARY_HOSTNAME \
          $WGET -q -O - --header=X-PSA-Blocking-Rewrite:psatest \
          $SECONDARY_HOST/mod_pagespeed_test/optimize_for_bandwidth/$1)
    check_from "$OUT" grep -q "$2"
    if [ "$#" -ge 3 ]; then
      check_from "$OUT" grep -q "$3"
    fi
  }
  test_optimize_for_bandwidth rewrite_css.html \
    '.blue{foreground-color:blue}body{background:url(arrow.png)}' \
    '<link rel="stylesheet" type="text/css" href="yellow.css">'
  test_optimize_for_bandwidth inline_css/rewrite_css.html \
    '.blue{foreground-color:blue}body{background:url(arrow.png)}' \
    '<style>.yellow{background-color:#ff0}</style>'
  test_optimize_for_bandwidth css_urls/rewrite_css.html \
    '.blue{foreground-color:blue}body{background:url(arrow.png)}' \
    '<link rel="stylesheet" type="text/css" href="A.yellow.css.pagespeed'
  test_optimize_for_bandwidth image_urls/rewrite_image.html \
    '<img src=\"xarrow.png.pagespeed.'
  test_optimize_for_bandwidth core_filters/rewrite_css.html \
    '.blue{foreground-color:blue}body{background:url(xarrow.png.pagespeed.' \
    '<style>.yellow{background-color:#ff0}</style>'

  # Make sure that optimize for bandwidth + CombineCSS doesn't eat
  # URLs.
  URL=http://optimizeforbandwidth.example.com/mod_pagespeed_example
  URL=$URL/combine_css.html?PageSpeedFilters=+combine_css
  OUT=$(http_proxy=$SECONDARY_HOSTNAME \
        $WGET -q -O - --header=X-PSA-Blocking-Rewrite:psatest $URL)
  check_from "$OUT" fgrep -q bold.css

  # Same for CombineJS --- which never actually did, to best of my knowledge,
  # but better check just in case.
  URL=http://optimizeforbandwidth.example.com/mod_pagespeed_example
  URL=$URL/combine_javascript.html?PageSpeedFilters=+combine_javascript
  OUT=$(http_proxy=$SECONDARY_HOSTNAME \
        $WGET -q -O - --header=X-PSA-Blocking-Rewrite:psatest $URL)
  check_from "$OUT" fgrep -q combine_javascript2

  # Test that we work fine with an explicitly configured SHM metadata cache.
  start_test Using SHM metadata cache
  HOST_NAME="http://shmcache.example.com"
  URL="$HOST_NAME/mod_pagespeed_example/rewrite_images.html"
  http_proxy=$SECONDARY_HOSTNAME fetch_until $URL 'grep -c .pagespeed.ic' 2

  # Test max_cacheable_response_content_length.  There are two Javascript files
  # in the html file.  The smaller Javascript file should be rewritten while
  # the larger one shouldn't.
  start_test Maximum length of cacheable response content.
  HOST_NAME="http://max-cacheable-content-length.example.com"
  DIR_NAME="mod_pagespeed_test/max_cacheable_content_length"
  HTML_NAME="test_max_cacheable_content_length.html"
  URL=$HOST_NAME/$DIR_NAME/$HTML_NAME
  RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --header \
      'X-PSA-Blocking-Rewrite: psatest' $URL)
  check_from     "$RESPONSE_OUT" fgrep -qi small.js.pagespeed.
  check_not_from "$RESPONSE_OUT" fgrep -qi large.js.pagespeed.

  start_test LoadFromFile with length limits
  # lff-large-files* have length limits set so that bold.css can be loaded from
  # file, but big.css cannot be.

  BASE="http://lff-large-files.example.com/mod_pagespeed_example/styles"
  URL="$BASE/bold.css"
  # Loads fine, as expected, because it's small.
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL

  URL="$BASE/big.css"
  # Still loads fine, because after we blocked LFF we DECLINED the request and
  # loaded it through the non-LFF path.
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL

  HOST="http://lff-large-files-no-fallback.example.com"
  URL="$HOST/bold.css"
  # Loads fine, as expected, because it's small.
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL

  URL="$HOST/big.css"
  # Doesn't load at all, because it's too big and we've configured a LFF path
  # that's different from the one you'd get without LFF.
  http_proxy=$SECONDARY_HOSTNAME check_not $WGET_DUMP $URL

  # This test checks that the XHeaderValue directive works.
  start_test XHeaderValue directive

  RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    http://xheader.example.com/mod_pagespeed_example)
  check_from "$RESPONSE_OUT" \
    egrep -q "X-(Page-Speed|Mod-Pagespeed): UNSPECIFIED VERSION"

  # This test checks that the DomainRewriteHyperlinks directive
  # can turn off.  See mod_pagespeed_test/rewrite_domains.html: it has
  # one <img> URL, one <form> URL, and one <a> url, all referencing
  # src.example.com.  Only the <img> url should be rewritten.
  start_test RewriteHyperlinks off directive
  HOST_NAME="http://domain-hyperlinks-off.example.com"
  RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      $HOST_NAME/mod_pagespeed_test/rewrite_domains.html)
  MATCHES=$(echo "$RESPONSE_OUT" | fgrep -c http://dst.example.com)
  check [ $MATCHES -eq 1 ]

  # This test checks that the DomainRewriteHyperlinks directive
  # can turn on.  See mod_pagespeed_test/rewrite_domains.html: it has
  # one <img> URL, one <form> URL, and one <a> url, all referencing
  # src.example.com.  They should all be rewritten to dst.example.com.
  start_test RewriteHyperlinks on directive
  HOST_NAME="http://domain-hyperlinks-on.example.com"
  RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      $HOST_NAME/mod_pagespeed_test/rewrite_domains.html)
  MATCHES=$(echo "$RESPONSE_OUT" | fgrep -c http://dst.example.com)
  check [ $MATCHES -eq 4 ]

  start_test static asset urls are mapped/sharded

  HOST_NAME="http://map-static-domain.example.com"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      $HOST_NAME/mod_pagespeed_example/rewrite_javascript.html)
  check_from "$OUT" fgrep \
    "http://static-cdn.example.com/$PSA_JS_LIBRARY_URL_PREFIX/js_defer"

  # Test to make sure dynamically defined url-valued attributes are rewritten by
  # rewrite_domains.  See mod_pagespeed_test/rewrite_domains.html: in addition
  # to having one <img> URL, one <form> URL, and one <a> url it also has one
  # <span src=...> URL, one <hr imgsrc=...> URL, one <hr src=...> URL, and one
  # <blockquote cite=...> URL, all referencing src.example.com.  The first three
  # should be rewritten because of hardcoded rules, the span.src and hr.imgsrc
  # should be rewritten because of UrlValuedAttribute directives, the hr.src
  # should be left unmodified, and the blockquote.src should be rewritten as an
  # image because of a UrlValuedAttribute override.  The rewritten ones should
  # all be rewritten to dst.example.com.
  HOST_NAME="http://url-attribute.example.com"
  TEST="$HOST_NAME/mod_pagespeed_test"
  REWRITE_DOMAINS="$TEST/rewrite_domains.html"
  UVA_EXTEND_CACHE="$TEST/url_valued_attribute_extend_cache.html"
  UVA_EXTEND_CACHE+="?PageSpeedFilters=core,+left_trim_urls"

  start_test Rewrite domains in dynamically defined url-valued attributes.

  RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $REWRITE_DOMAINS)
  MATCHES=$(echo "$RESPONSE_OUT" | fgrep -c http://dst.example.com)
  check [ $MATCHES -eq 6 ]
  MATCHES=$(echo "$RESPONSE_OUT" | \
      fgrep -c '<hr src=http://src.example.com/hr-image>')
  check [ $MATCHES -eq 1 ]

  start_test Additional url-valued attributes are fully respected.

  function count_exact_matches() {
    # Needed because "fgrep -c" counts lines with matches, not pure matches.
    fgrep -o "$1" | wc -l
  }

  # There are ten resources that should be optimized.
  http_proxy=$SECONDARY_HOSTNAME \
      fetch_until $UVA_EXTEND_CACHE 'count_exact_matches .pagespeed.' 10

  # Make sure <custom d=...> isn't modified at all, but that everything else is
  # recognized as a url and rewritten from ../foo to /foo.  This means that only
  # one reference to ../mod_pagespeed should remain, <custom d=...>.
  http_proxy=$SECONDARY_HOSTNAME \
      fetch_until $UVA_EXTEND_CACHE 'grep -c d=.[.][.]/mod_pa' 1
  http_proxy=$SECONDARY_HOSTNAME \
      fetch_until $UVA_EXTEND_CACHE 'fgrep -c ../mod_pa' 1

  # There are ten images that should be optimized, so grep including .ic.
  http_proxy=$SECONDARY_HOSTNAME \
      fetch_until $UVA_EXTEND_CACHE 'count_exact_matches .pagespeed.ic' 10

  start_test url-valued stylesheet attributes are properly handled

  function url_valued_attribute_css_optimization_status() {
    local input=$(cat)
    if [[ $(echo "$input" | fgrep -o ".pagespeed.cf." | wc -l) != 7 ]]; then
      echo incomplete  # still some unoptimized css files
    elif ! echo "$input" | \
             grep -q "<style>.bold{font-weight:bold}</style>"; then
      echo incomplete  # bold.css still not inlined
    else
      echo complete
    fi
  }

  URL="$TEST/url_valued_attribute_css.html"
  URL+="?PageSpeedFilters=debug,combine_css,rewrite_css,inline_css"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
    url_valued_attribute_css_optimization_status complete
  OUT=$(cat $FETCH_UNTIL_OUTFILE)
  check_from "$OUT" grep \
    "Could not combine over barrier: custom or alternate stylesheet attribute"
  check_from "$OUT" grep 'link data-stylesheet=[^<]*.pagespeed.cf'
  LOOK_FOR="<span data-stylesheet-a=[^<]*.pagespeed.cf"
  LOOK_FOR+="[^<]*data-stylesheet-b=[^<]*.pagespeed.cf"
  LOOK_FOR+="[^<]*data-stylesheet-c=[^<]*.pagespeed.cf"
  check_from "$OUT" grep "$LOOK_FOR"
  check_from "$OUT" grep "<link rel=invalid data-stylesheet=[^<]*.pagespeed.cf"
  check_from "$OUT" grep \
    "<style data-stylesheet=[^<]*.pagespeed.cf[^>]*>.bold{font-weight:bold}"
  check_not_from "$OUT" fgrep "blue.css+yellow.css"

  start_test load from file with ipro
  URL="http://lff-ipro.example.com/mod_pagespeed_test/lff_ipro/fake.woff"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -O - $URL)
  check_from "$OUT" grep "^This isn't really a woff file\.$"
  check [ "$(echo "$OUT" | wc -l)" = 1 ]

  start_test max cacheable content length with ipro
  URL="http://max-cacheable-content-length.example.com/mod_pagespeed_example/"
  URL+="images/BikeCrashIcn.png"
  # This used to check-fail the server; see ngx_pagespeed issue #771.
  http_proxy=$SECONDARY_HOSTNAME check $WGET -t 1 -O /dev/null $URL

  readonly EXP_DEVICES_EXAMPLE="http://experiment.devicematch.example.com/mod_pagespeed_example"
  readonly EXP_DEVICES_EXTEND_CACHE="$EXP_DEVICES_EXAMPLE/extend_cache.html"

  readonly DESKTOP_UA="Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) AppleWebKit/534.13 (KHTML, like Gecko) Chrome/18.0.597.19 Safari/534.13"
  readonly MOBILE_UA="Mozilla/5.0 (Linux; Android 4.1.4; Galaxy Nexus Build/IMM76B) AppleWebKit/535.19 (KHTML, like Gecko) Chrome/21.0.1025.133 Mobile Safari/535.19"

  start_test Mobile experiment does not match desktop device.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -U "$DESKTOP_UA" \
        $EXP_DEVICES_EXTEND_CACHE)
  check_from "$OUT" grep -q 'Set-Cookie: PageSpeedExperiment=0;'

  start_test Mobile experiment matches mobile device.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -U "$MOBILE_UA" \
        $EXP_DEVICES_EXTEND_CACHE)
  check_from "$OUT" grep -q 'Set-Cookie: PageSpeedExperiment=1;'

  start_test Can force-enroll in experment for wrong device type.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP -U "$DESKTOP_UA" \
        $EXP_DEVICES_EXTEND_CACHE?PageSpeedEnrollExperiment=1)
  check_from "$OUT" grep -q 'Set-Cookie: PageSpeedExperiment=1;'

  start_test Downstream cache integration caching headers.
  URL="http://downstreamcacheresource.example.com/mod_pagespeed_example/images/"
  URL+="xCuppa.png.pagespeed.ic.0.png"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $URL)
  check_from "$OUT" egrep -iq $'^Cache-Control: .*\r$'
  check_from "$OUT" egrep -iq $'^Expires: .*\r$'
  check_from "$OUT" egrep -iq $'^Last-Modified: .*\r$'

  # Test that we do not rewrite resources when the X-Sendfile header is set, or
  # when the X-Accel-Redirect header is set.
  start_test check that rewriting only happens without X-Sendfile
  function verify_no_rewriting_sendfile() {
    local sendfile_hostname=$1
    local sendfile_header=$2
    URL="http://${sendfile_hostname}.example.com/mod_pagespeed_test/normal.js"
    OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
      --header 'X-PSA-Blocking-Rewrite: psatest' --save-headers $URL)
    check_from "$OUT" grep $sendfile_header
    check_from "$OUT" grep comment2
  }
  verify_no_rewriting_sendfile "uses-sendfile" "X-Sendfile"
  verify_no_rewriting_sendfile "uses-xaccelredirect" "X-Accel-Redirect"
  # doesnt-sendfile.example.com has identical configuration, but just does not
  # set the X-Sendfile header. Check this here to make sure that we have do
  # rewrite under other circumstances.
  URL="http://doesnt-sendfile.example.com/mod_pagespeed_test/normal.js"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      'fgrep -c comment2' 0

  # Test a scenario where a multi-domain installation is using a
  # single CDN for all hosts, and uses a subdirectory in the CDN to
  # distinguish hosts.  Some of the resources may already be mapped to
  # the CDN in the origin HTML, but we want to fetch them directly
  # from localhost.  If we do this successfully (see the MapOriginDomain
  # command in customhostheader.example.com in the configuration), we will
  # inline a small image.
  start_test shared CDN short-circuit back to origin via host-header override
  URL="http://customhostheader.example.com/map_origin_host_header.html"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$URL" \
      "grep -c data:image/png;base64" 1

  # Optimize in-place images for browser. Ideal test matrix (not covered yet):
  # User-Agent:  Accept:  Image type   Result
  # -----------  -------  ----------   ----------------------------------
  #    IE         N/A     photo        image/jpeg, Cache-Control: private *
  #     :         N/A     synthetic    image/png,  no vary
  #  Old Opera     no     photo        image/jpeg, Vary: Accept
  #     :          no     synthetic    image/png,  no vary
  #     :         webp    photo        image/webp, Vary: Accept, Lossy
  #     :         webp    synthetic    image/png,  no vary
  #  Chrome or     no     photo        image/jpeg, Vary: Accept
  # Firefox or     no     synthetic    image/png,  no vary
  #  New Opera    webp    photo        image/webp, Vary: Accept, Lossy
  #     :         webp    synthetic    image/png,  no vary
  # TODO(jmaessen): * cases currently send Vary: Accept.  Fix (in progress).
  # TODO(jmaessen): Send image/webp lossless for synthetic and alpha-channel
  # images.  Will require reverting to Vary: Accept for these.  Stuff like
  # animated webp will have to remain unconverted still in IPRO mode, or switch
  # to cc: private, but right now animated webp support is still pending anyway.
  function test_ipro_for_browser_webp() {
    IN_UA_PRETTY="$1"; shift
    IN_UA="$1"; shift
    IN_ACCEPT="$1"; shift
    IMAGE_TYPE="$1"; shift
    OUT_CONTENT_TYPE="$1"; shift
    OUT_VARY="${1-}"; shift || true
    OUT_CC="${1-}"; shift || true

    # Remaining args are the expected headers (Name:Value), photo, or synthetic.
    if [ "$IMAGE_TYPE" = "photo" ]; then
      URL="http://ipro-for-browser.example.com/images/Puzzle.jpg"
    else
      URL="http://ipro-for-browser.example.com/images/Cuppa.png"
    fi
    TEST_ID="In-place optimize for "
    TEST_ID+="User-Agent:${IN_UA_PRETTY:-${IN_UA:-None}},"
    if [ -z "$IN_ACCEPT" ]; then
      TEST_ID+=" no accept, "
    else
      TEST_ID+=" Accept:$IN_ACCEPT, "
    fi
    TEST_ID+=" $IMAGE_TYPE.  Expect image/${OUT_CONTENT_TYPE}, "
    if [ -z "$OUT_VARY" ]; then
      TEST_ID+=" no vary, "
    else
      TEST_ID+=" Vary:${OUT_VARY}, "
    fi
    if [ -z "$OUT_CC" ]; then
      TEST_ID+=" cacheable."
    else
      TEST_ID+=" Cache-Control:${OUT_CC}."
    fi
    start_test $TEST_ID
    http_proxy=$SECONDARY_HOSTNAME \
      fetch_until -save $URL 'grep -c W/\"PSA-aj-' 1 \
          "--save-headers \
          ${IN_UA:+--user-agent $IN_UA} \
          ${IN_ACCEPT:+--header=Accept:image/$IN_ACCEPT}"
    check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
      fgrep -q "Content-Type: image/$OUT_CONTENT_TYPE"
    if [ -z "$OUT_VARY" ]; then
      check_not_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
        fgrep -q "Vary:"
    else
      check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
        fgrep -q "Vary: $OUT_VARY"
    fi
    check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
      grep -q "Cache-Control: ${OUT_CC:-max-age=[0-9]*}$"
    # TODO(jmaessen): check file type of webp.  Irrelevant for now.
  }

  ##############################################################################
  # Test with testing-only user agent strings.
  #                          UA           Accept Type  Out  Vary     CC
  test_ipro_for_browser_webp "None" ""    ""     photo jpeg "Accept"
  test_ipro_for_browser_webp "" "webp"    ""     photo jpeg "Accept"
  test_ipro_for_browser_webp "" "webp-la" ""     photo jpeg "Accept"
  test_ipro_for_browser_webp "None" ""    "webp" photo webp "Accept"
  test_ipro_for_browser_webp "" "webp"    "webp" photo webp "Accept"
  test_ipro_for_browser_webp "" "webp-la" "webp" photo webp "Accept"
  test_ipro_for_browser_webp "None" ""    ""     synth png
  test_ipro_for_browser_webp "" "webp"    ""     synth png
  test_ipro_for_browser_webp "" "webp-la" ""     synth png
  test_ipro_for_browser_webp "None" ""    "webp" synth png
  test_ipro_for_browser_webp "" "webp"    "webp" synth png
  test_ipro_for_browser_webp "" "webp-la" "webp" synth png
  ##############################################################################

  # Wordy UAs need to be stored in the WGETRC file to avoid death by quoting.
  OLD_WGETRC=$WGETRC
  WGETRC=$TESTTMP/wgetrc-ua
  export WGETRC

  # IE 9 and later must re-validate Vary: Accept.  We should send CC: private.
  IE9_UA="Mozilla/5.0 (Windows; U; MSIE 9.0; WIndows NT 9.0; en-US))"
  IE11_UA="Mozilla/5.0 (Windows NT 6.1; WOW64; ***********; rv:11.0) like Gecko"
  echo "user_agent = $IE9_UA" > $WGETRC
  #                           (no accept)  Type  Out  Vary CC
  test_ipro_for_browser_webp "IE 9"  "" "" photo jpeg ""   "max-age=[0-9]*,private"
  test_ipro_for_browser_webp "IE 9"  "" "" synth png
  echo "user_agent = $IE11_UA" > $WGETRC
  test_ipro_for_browser_webp "IE 11" "" "" photo jpeg ""   "max-age=[0-9]*,private"
  test_ipro_for_browser_webp "IE 11" "" "" synth png

  # Older Opera did not support webp.
  OPERA_UA="Opera/9.80 (Windows NT 5.2; U; en) Presto/2.7.62 Version/11.01"
  echo "user_agent = $OPERA_UA" > $WGETRC
  #                                (no accept) Type  Out  Vary
  test_ipro_for_browser_webp "Old Opera" "" "" photo jpeg "Accept"
  test_ipro_for_browser_webp "Old Opera" "" "" synth png
  # Slightly newer opera supports only lossy webp, sends header.
  OPERA_UA="Opera/9.80 (Windows NT 6.0; U; en) Presto/2.8.99 Version/11.10"
  echo "user_agent = $OPERA_UA" > $WGETRC
  #                                           Accept Type  Out  Vary
  test_ipro_for_browser_webp "Newer Opera" "" "webp" photo webp "Accept"
  test_ipro_for_browser_webp "Newer Opera" "" "webp" synth png

  function test_decent_browsers() {
    echo "user_agent = $2" > $WGETRC
    #                          UA      Accept Type      Out  Vary
    test_ipro_for_browser_webp "$1" "" ""     photo     jpeg "Accept"
    test_ipro_for_browser_webp "$1" "" ""     synthetic  png
    test_ipro_for_browser_webp "$1" "" "webp" photo     webp "Accept"
    test_ipro_for_browser_webp "$1" "" "webp" synthetic  png
  }
  CHROME_UA="Mozilla/5.0 (Macintosh; Intel Mac OS X 10_9_1) AppleWebKit/537.36 "
  CHROME_UA+="(KHTML, like Gecko) Chrome/32.0.1700.102 Safari/537.36"
  test_decent_browsers "Chrome" "$CHROME_UA"
  FIREFOX_UA="Mozilla/5.0 (X11; U; Linux x86_64; zh-CN; rv:1.9.2.10) "
  FIREFOX_UA+="Gecko/20100922 Ubuntu/10.10 (maverick) Firefox/3.6.10"
  test_decent_browsers "Firefox" "$FIREFOX_UA"
  test_decent_browsers "New Opera" \
    "Opera/9.80 (Windows NT 6.0) Presto/2.12.388 Version/12.14"

  WGETRC=$OLD_WGETRC

  start_test Request Option Override : Correct values are passed
  HOST_NAME="http://request-option-override.example.com"
  OPTS="?ModPagespeed=on"
  OPTS+="&ModPagespeedFilters=+collapse_whitespace,+remove_comments"
  OPTS+="&PageSpeedRequestOptionOverride=abc"
  URL="$HOST_NAME/mod_pagespeed_test/forbidden.html$OPTS"
  OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)"
  echo wget $URL
  check_not_from "$OUT" grep -q '<!--'

  start_test Request Option Override : Incorrect values are passed
  HOST_NAME="http://request-option-override.example.com"
  OPTS="?ModPagespeed=on"
  OPTS+="&ModPagespeedFilters=+collapse_whitespace,+remove_comments"
  OPTS+="&PageSpeedRequestOptionOverride=notabc"
  URL="$HOST_NAME/mod_pagespeed_test/forbidden.html$OPTS"
  OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)"
  echo wget $URL
  check_from "$OUT" grep -q '<!--'

  start_test Request Option Override : Correct values are passed as headers
  HOST_NAME="http://request-option-override.example.com"
  OPTS="--header=ModPagespeed:on"
  OPTS+=" --header=ModPagespeedFilters:+collapse_whitespace,+remove_comments"
  OPTS+=" --header=PageSpeedRequestOptionOverride:abc"
  URL="$HOST_NAME/mod_pagespeed_test/forbidden.html"
  echo wget $OPTS $URL
  OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $OPTS $URL)"
  check_not_from "$OUT" grep -q '<!--'

  start_test Request Option Override : Incorrect values are passed as headers
  HOST_NAME="http://request-option-override.example.com"
  OPTS="--header=ModPagespeed:on"
  OPTS+=" --header=ModPagespeedFilters:+collapse_whitespace,+remove_comments"
  OPTS+=" --header=PageSpeedRequestOptionOverride:notabc"
  URL="$HOST_NAME/mod_pagespeed_test/forbidden.html"
  echo wget $OPTS $URL
  OUT="$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $OPTS $URL)"
  check_from "$OUT" grep -q '<!--'

  start_test IPRO flow uses cache as expected.
  # TODO(sligocki): Use separate VHost instead to separate stats.
  STATS=$OUTDIR/blocking_rewrite_stats
  IPRO_HOST=http://ipro.example.com
  IPRO_ROOT=$IPRO_HOST/mod_pagespeed_test/ipro
  URL=$IPRO_ROOT/test_image_dont_reuse2.png
  IPRO_STATS_URL=$IPRO_HOST/pagespeed_admin/statistics
  OUTFILE=$OUTDIR/ipro_output

  # Initial stats.
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.0

  # First IPRO request.
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O /dev/null
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.1

  # Resource not in cache the first time.
  check_stat $STATS.0 $STATS.1 cache_hits 0
  check_stat $STATS.0 $STATS.1 cache_misses 1
  check_stat $STATS.0 $STATS.1 ipro_served 0
  check_stat $STATS.0 $STATS.1 ipro_not_rewritable 0
  # So we run the ipro recorder flow and insert it into the cache.
  check_stat $STATS.0 $STATS.1 ipro_not_in_cache 1
  check_stat $STATS.0 $STATS.1 ipro_recorder_resources 1
  check_stat $STATS.0 $STATS.1 ipro_recorder_inserted_into_cache 1
  # Image doesn't get rewritten the first time.
  # TODO(sligocki): This should change to 1 when we get image rewrites started
  # in the Apache output filter flow.
  check_stat $STATS.0 $STATS.1 image_rewrites 0

  # Second IPRO request.
  # Original file has content-length 15131.  Once ipro-optimized, it is
  # 11395, so fetch it until it's less than 12000.
  http_proxy=$SECONDARY_HOSTNAME fetch_until $URL "wc -c" 12000 "" "-lt"
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.2

  # Resource is found in cache the second time.
  check_stat_op $STATS.1 $STATS.2 cache_hits 1 -ge
  check_stat_op $STATS.1 $STATS.2 ipro_served 1 -ge
  check_stat $STATS.1 $STATS.2 ipro_not_rewritable 0
  # So we don't run the ipro recorder flow.
  check_stat $STATS.1 $STATS.2 ipro_not_in_cache 0
  check_stat $STATS.1 $STATS.2 ipro_recorder_resources 0
  # Image gets rewritten on the second pass through this filter.
  # TODO(sligocki): This should change to 0 when we get image rewrites started
  # in the Apache output filter flow.
  #
  # Note also that image_rewrite stats are inherently flaky because locks are
  # advisory, and steals may occur in valgrind, so we check for image rewrites
  # being in the range 1:2.
  check_stat_op $STATS.1 $STATS.2 image_rewrites 1 -ge
  check_stat_op $STATS.1 $STATS.2 image_rewrites 2 -le

  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O $OUTFILE
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.3

  check_stat $STATS.2 $STATS.3 cache_hits 1
  check_stat $STATS.2 $STATS.3 ipro_served 1
  check_stat $STATS.2 $STATS.3 ipro_recorder_resources 0
  # Allow some slop in image_rewrites stat due to, I suspect, advisory locks.
  check_stat_op $STATS.2 $STATS.3 image_rewrites 1 -le

  # Check that the IPRO served file didn't discard any Apache err_response_out
  # headers.  Only do this on servers that support err_response_out, so first
  # check that X-TestHeader is ever set.
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_ROOT/)
  check_from "$OUT" fgrep "Content-Type: text/html"
  if echo "$OUT" | grep -q "^X-TestHeader:"; then
    check_from "$(extract_headers $OUTFILE)" grep -q "X-TestHeader: hello"
  fi

  start_test "IPRO flow doesn't copy uncacheable resources multiple times."
  URL=$IPRO_ROOT/nocache/test_image_dont_reuse.png

  # Initial stats.
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.0

  # First IPRO request.
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O /dev/null
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.1

  # Resource not in cache the first time.
  check_stat $STATS.0 $STATS.1 cache_hits 0
  check_stat $STATS.0 $STATS.1 cache_misses 1
  check_stat $STATS.0 $STATS.1 ipro_served 0
  check_stat $STATS.0 $STATS.1 ipro_not_rewritable 0
  # So we run the ipro recorder flow, but the resource is not cacheable.
  check_stat $STATS.0 $STATS.1 ipro_not_in_cache 1
  check_stat $STATS.0 $STATS.1 ipro_recorder_resources 1
  check_stat $STATS.0 $STATS.1 ipro_recorder_not_cacheable 1
  # Uncacheable, so no rewrites.
  check_stat $STATS.0 $STATS.1 image_rewrites 0
  check_stat $STATS.0 $STATS.1 image_ongoing_rewrites 0

  # Second IPRO request.
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O /dev/null
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.2

  check_stat $STATS.1 $STATS.2 cache_hits 0
  # Note: This should load a RecentFetchFailed record from cache, but that
  # is reported as a cache miss.
  check_stat $STATS.1 $STATS.2 cache_misses 1
  check_stat $STATS.1 $STATS.2 ipro_served 0
  check_stat $STATS.1 $STATS.2 ipro_not_rewritable 1
  # Important: We do not record this resource the second and third time
  # because we remember that it was not cacheable.
  check_stat $STATS.1 $STATS.2 ipro_not_in_cache 0
  check_stat $STATS.1 $STATS.2 ipro_recorder_resources 0
  check_stat $STATS.1 $STATS.2 image_rewrites 0
  check_stat $STATS.1 $STATS.2 image_ongoing_rewrites 0

  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $URL -O /dev/null
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $IPRO_STATS_URL > $STATS.3

  # Same as second fetch.
  check_stat $STATS.2 $STATS.3 cache_hits 0
  check_stat $STATS.2 $STATS.3 cache_misses 1
  check_stat $STATS.2 $STATS.3 ipro_not_rewritable 1
  check_stat $STATS.2 $STATS.3 ipro_recorder_resources 0
  check_stat $STATS.2 $STATS.3 image_rewrites 0
  check_stat $STATS.2 $STATS.3 image_ongoing_rewrites 0

  # Check that IPRO served resources that don't specify a cache control
  # value are given the TTL specified by the ImplicitCacheTtlMs directive.
  start_test "IPRO respects ImplicitCacheTtlMs."
  HTML_URL=$IPRO_ROOT/no-cache-control-header/ipro.html
  RESOURCE_URL=$IPRO_ROOT/no-cache-control-header/test_image_dont_reuse.png
  RESOURCE_HEADERS=$OUTDIR/resource_headers
  OUTFILE=$OUTDIR/ipro_resource_output

  # Fetch the HTML to initiate rewriting and caching of the image.
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $HTML_URL -O $OUTFILE

  # First IPRO resource request after a short wait: it will never be optimized
  # because our non-load-from-file flow doesn't support that, but it will have
  # the full TTL.
  sleep 2
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $RESOURCE_URL -O $OUTFILE
  check_file_size "$OUTFILE" -gt 15000 # not optimized
  RESOURCE_MAX_AGE=$( \
    extract_headers $OUTFILE | \
    grep 'Cache-Control:' | tr -d '\r' | \
    sed -e 's/^ *Cache-Control: *//' | sed -e 's/^.*max-age=\([0-9]*\).*$/\1/')
  check test -n "$RESOURCE_MAX_AGE"
  check test $RESOURCE_MAX_AGE -eq 333

  # Second IPRO resource request after a short wait: it will still be optimized
  # and the TTL will be reduced.
  sleep 2
  http_proxy=$SECONDARY_HOSTNAME check $WGET_DUMP $RESOURCE_URL -O $OUTFILE
  check_file_size "$OUTFILE" -lt 15000 # optimized
  RESOURCE_MAX_AGE=$( \
    extract_headers $OUTFILE | \
    grep 'Cache-Control:' | tr -d '\r' | \
    sed -e 's/^ *Cache-Control: *//' | sed -e 's/^.*max-age=\([0-9]*\).*$/\1/')
  check test -n "$RESOURCE_MAX_AGE"

  check test $RESOURCE_MAX_AGE -lt 333
  check test $RESOURCE_MAX_AGE -gt 300

  # This test checks that the ClientDomainRewrite directive can turn on.
  start_test ClientDomainRewrite on directive
  HOST_NAME="http://client-domain-rewrite.example.com"
  RESPONSE_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
    $HOST_NAME/mod_pagespeed_test/rewrite_domains.html)
  MATCHES=$(echo "$RESPONSE_OUT" | grep -c pagespeed\.clientDomainRewriterInit)
  check [ $MATCHES -eq 1 ]

  # Verify rendered image dimensions test.
  start_test resize_rendered_image_dimensions with critical images beacon
  HOST_NAME="http://renderedimagebeacon.example.com"
  URL="$HOST_NAME/mod_pagespeed_test/image_rewriting/image_resize_using_rendered_dimensions.html"
  http_proxy=$SECONDARY_HOSTNAME \
      fetch_until -save -recursive $URL 'fgrep -c "data-pagespeed-url-hash"' 2 \
      '--header=X-PSA-Blocking-Rewrite:psatest'
  check [ $(grep -c "^pagespeed\.CriticalImages\.Run" \
    $WGET_DIR/image_resize_using_rendered_dimensions.html) = 1 ];
  OPTIONS_HASH=$(\
    awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-3)}' \
    $WGET_DIR/image_resize_using_rendered_dimensions.html)
  NONCE=$(awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-1)}' \
          $WGET_DIR/image_resize_using_rendered_dimensions.html)

  # Send a beacon response using POST indicating that OptPuzzle.jpg is
  # critical and has rendered dimensions.
  BEACON_URL="$HOST_NAME/$BEACON_HANDLER"
  BEACON_URL+="?url=http%3A%2F%2Frenderedimagebeacon.example.com%2Fmod_pagespeed_test%2F"
  BEACON_URL+="image_rewriting%2Fimage_resize_using_rendered_dimensions.html"
  BEACON_DATA="oh=$OPTIONS_HASH&n=$NONCE&ci=1344500982&rd=%7B%221344500982%22%3A%7B%22rw%22%3A150%2C%22rh%22%3A100%2C%22ow%22%3A256%2C%22oh%22%3A192%7D%7D"
  OUT=$(env http_proxy=$SECONDARY_HOSTNAME \
    $CURL -sSi -d "$BEACON_DATA" "$BEACON_URL")
  check_from "$OUT" egrep -q "HTTP/1[.]. 204"
  http_proxy=$SECONDARY_HOSTNAME \
    fetch_until -save -recursive $URL \
    'fgrep -c 150x100xOptPuzzle.jpg.pagespeed.ic.' 1

  if [ -z "${DISABLE_PHP_TESTS:-}" ]; then
    function php_ipro_record() {
      local url="$1"
      local max_cache_bytes="$2"
      local cache_bytes_op="$3"
      local cache_bytes_cmp="$4"
      echo http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $url
      OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --save-headers $url)
      check_from "$OUT" egrep -iq 'Content-Encoding: gzip'
      # Now check that we receive the optimized content from cache (not double
      # zipped). When the resource is optimized, "peachpuff" is rewritten to its
      # hex equivalent.
      http_proxy=$SECONDARY_HOSTNAME fetch_until $url 'fgrep -c ffdab9' 1
      # The gzipped size is 175 with the default compression. Our compressed
      # cache uses gzip -9, which will compress even better (below 150).
      OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -O - \
        --header="Accept-Encoding: gzip" $url)
      # TODO(jcrowell): clean up when ngx_pagespeed stops chunk-encoding static
      # resources.
      bytes_from_cache=$(echo "$OUT" | wc -c)
      check [ $bytes_from_cache -lt $max_cache_bytes ]
      check [ $bytes_from_cache $cache_bytes_op $cache_bytes_cmp ]
      # Ensure that the Content-Encoding header matches the data that is sent.
      OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
        --header='Accept-Encoding: gzip' $url \
        | scrape_header 'Content-Encoding')
      check_from "$OUT" fgrep -q 'gzip'
      OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET -O - $url)
      bytes_from_cache_uncompressed=$(echo "$OUT" | wc -c)
      # The data should uncompressed, but minified at this point.
      check [ $bytes_from_cache_uncompressed -gt 10000 ]
      check_from "$OUT" grep -q "ffdab9"
      # Ensure that the Content-Encoding header matches the data that is sent.
      # In this case we didn't sent the Accept-Encoding header, so we don't
      # expect the data to have any Content-Encoding header.
      OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $url)
      check_not_from "$OUT" egrep -q 'Content-Encoding'
    }
    start_test Cache Compression On: PHP Pre-Gzipping does not get destroyed by the cache.
    URL="http://compressedcache.example.com/mod_pagespeed_test/php_gzip.php"
    # The gzipped size is 175 with the default compression. Our compressed
    # cache uses gzip -9, which will compress even better (below 150).
    php_ipro_record "$URL" 150 "-lt" 175
    start_test Cache Compression Off: PHP Pre-Gzipping does not get destroyed by the cache.
    # With cache compression off, we should see about 175 for both the pre and
    # post optimized resource.
    URL="http://uncompressedcache.example.com/mod_pagespeed_test/php_gzip.php"
    php_ipro_record "$URL" 200 "-gt" 150
  fi

  # Verify that downstream caches and rebeaconing interact correctly for images.
  start_test lazyload_images,rewrite_images with downstream cache rebeaconing
  HOST_NAME="http://downstreamcacherebeacon.example.com"
  URL="$HOST_NAME/mod_pagespeed_test/downstream_caching.html"
  URL+="?PageSpeedFilters=lazyload_images"
  # 1. Even with blocking rewrite, we don't get an instrumented page when the
  # PS-ShouldBeacon header is missing.
  OUT1=$(http_proxy=$SECONDARY_HOSTNAME \
            $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL)
  check_not_from "$OUT1" egrep -q 'pagespeed\.CriticalImages\.Run'
  check_from "$OUT1" grep -q "Cache-Control: private, max-age=3000"
  # 2. We get an instrumented page if the correct key is present.
  OUT2=$(http_proxy=$SECONDARY_HOSTNAME \
            $WGET_DUMP $WGET_ARGS \
            --header="X-PSA-Blocking-Rewrite: psatest" \
            --header="PS-ShouldBeacon: random_rebeaconing_key" $URL)
  check_from "$OUT2" egrep -q "pagespeed\.CriticalImages\.Run"
  check_from "$OUT2" grep -q "Cache-Control: max-age=0, no-cache"
  # 3. We do not get an instrumented page if the wrong key is present.
  OUT3=$(http_proxy=$SECONDARY_HOSTNAME \
            $WGET_DUMP $WGET_ARGS \
            --header="X-PSA-Blocking-Rewrite: psatest" \
            --header="PS-ShouldBeacon: wrong_rebeaconing_key" $URL)
  check_not_from "$OUT3" egrep -q "pagespeed\.CriticalImages\.Run"
  check_from "$OUT3" grep -q "Cache-Control: private, max-age=3000"

  # Verify that downstream caches and rebeaconing interact correctly for css.
  test_filter prioritize_critical_css
  HOST_NAME="http://downstreamcacherebeacon.example.com"
  URL="$HOST_NAME/mod_pagespeed_test/downstream_caching.html"
  URL+="?PageSpeedFilters=prioritize_critical_css"
  # 1. Even with blocking rewrite, we don't get an instrumented page when the
  # PS-ShouldBeacon header is missing.
  OUT1=$(http_proxy=$SECONDARY_HOSTNAME \
            $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL)
  check_not_from "$OUT1" egrep -q 'pagespeed\.criticalCssBeaconInit'
  check_from "$OUT1" grep -q "Cache-Control: private, max-age=3000"

  # 2. We get an instrumented page if the correct key is present.
  http_proxy=$SECONDARY_HOSTNAME \
    fetch_until -save $URL 'grep -c criticalCssBeaconInit' 2 \
    "--header=PS-ShouldBeacon:random_rebeaconing_key --save-headers"
  check grep -q "Cache-Control: max-age=0, no-cache" $FETCH_UNTIL_OUTFILE

  # 3. We do not get an instrumented page if the wrong key is present.
  OUT3=$(http_proxy=$SECONDARY_HOSTNAME \
            $WGET_DUMP \
            --header 'PS-ShouldBeacon: wrong_rebeaconing_key' \
            --header 'X-PSA-Blocking-Rewrite: psatest' \
            $URL)
  check_not_from "$OUT3" egrep -q "pagespeed\.criticalCssBeaconInit"
  check_from "$OUT3" grep -q "Cache-Control: private, max-age=3000"

  # Verify that we can send a critical image beacon and that lazyload_images
  # does not try to lazyload the critical images.
  start_test lazyload_images,rewrite_images with critical images beacon
  HOST_NAME="http://imagebeacon.example.com"
  URL="$HOST_NAME/mod_pagespeed_test/image_rewriting/rewrite_images.html"
  # There are 3 images on rewrite_images.html.  Since beaconing is on but we've
  # sent no beacon data, none should be lazy loaded.
  # Run until we see beaconing on the page (should happen on first visit).
  http_proxy=$SECONDARY_HOSTNAME\
    fetch_until -save $URL \
    'fgrep -c "pagespeed.CriticalImages.Run"' 1
  check [ $(grep -c "data-pagespeed-lazy-src=" $FETCH_FILE) = 0 ];
  # We need the options hash and nonce to send a critical image beacon, so
  # extract it from injected beacon JS.
  OPTIONS_HASH=$(
    awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-3)}' $FETCH_FILE)
  NONCE=$(
    awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-1)}' $FETCH_FILE)
  # Send a beacon response using POST indicating that Puzzle.jpg is a critical
  # image.
  BEACON_URL="$HOST_NAME/$BEACON_HANDLER"
  BEACON_URL+="?url=http%3A%2F%2Fimagebeacon.example.com%2Fmod_pagespeed_test%2F"
  BEACON_URL+="image_rewriting%2Frewrite_images.html"
  BEACON_DATA="oh=$OPTIONS_HASH&n=$NONCE&ci=2932493096"

  OUT=$(env http_proxy=$SECONDARY_HOSTNAME \
    $CURL -sSi -d  "$BEACON_DATA" "$BEACON_URL")
  check_from "$OUT" egrep -q "HTTP/1[.]. 204"
  # Now 2 of the images should be lazyloaded, Puzzle.jpg should not be.
  http_proxy=$SECONDARY_HOSTNAME \
    fetch_until -save -recursive $URL 'fgrep -c data-pagespeed-lazy-src=' 2

  # Now test sending a beacon with a GET request, instead of POST. Indicate that
  # Puzzle.jpg and Cuppa.png are the critical images. In practice we expect only
  # POSTs to be used by the critical image beacon, but both code paths are
  # supported.  We add query params to URL to ensure that we get an instrumented
  # page without blocking.
  URL="$URL?id=4"
  http_proxy=$SECONDARY_HOSTNAME\
    fetch_until -save $URL \
    'fgrep -c "pagespeed.CriticalImages.Run"' 1
  check [ $(grep -c "data-pagespeed-lazy-src=" $FETCH_FILE) = 0 ];
  OPTIONS_HASH=$(
    awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-3)}' $FETCH_FILE)
  NONCE=$(
    awk -F\' '/^pagespeed\.CriticalImages\.Run/ {print $(NF-1)}' $FETCH_FILE)
  BEACON_URL="$HOST_NAME/$BEACON_HANDLER"
  BEACON_URL+="?url=http%3A%2F%2Fimagebeacon.example.com%2Fmod_pagespeed_test%2F"
  BEACON_URL+="image_rewriting%2Frewrite_images.html%3Fid%3D4"
  BEACON_DATA="oh=$OPTIONS_HASH&n=$NONCE&ci=2932493096"
  # Add the hash for Cuppa.png to BEACON_DATA, which will be used as the query
  # params for the GET.
  BEACON_DATA+=",2644480723"
  OUT=$(env http_proxy=$SECONDARY_HOSTNAME \
    $CURL -sSi "$BEACON_URL&$BEACON_DATA")
  check_from "$OUT" egrep -q "HTTP/1[.]. 204"
  # Now only BikeCrashIcn.png should be lazyloaded.
  http_proxy=$SECONDARY_HOSTNAME \
    fetch_until -save -recursive $URL 'fgrep -c data-pagespeed-lazy-src=' 1

  test_filter prioritize_critical_css

  start_test no critical selectors chosen from unauthorized resources
  URL="$TEST_ROOT/unauthorized/prioritize_critical_css.html"
  URL+="?PageSpeedFilters=prioritize_critical_css,debug"
  fetch_until -save $URL 'fgrep -c pagespeed.criticalCssBeaconInit' 3
  # Except for the occurrence in html, the gsc-completion-selected string
  # should not occur anywhere else, i.e. in the selector list.
  check [ $(fgrep -c "gsc-completion-selected" $FETCH_FILE) -eq 1 ]
  # From the css file containing an unauthorized @import line,
  # a) no selectors from the unauthorized @ import (e.g .maia-display) should
  #    appear in the selector list.
  check_not fgrep -q "maia-display" $FETCH_FILE
  # b) no selectors from the authorized @ import (e.g .interesting_color) should
  #    appear in the selector list because it won't be flattened.
  check_not fgrep -q "interesting_color" $FETCH_FILE
  # c) selectors that don't depend on flattening should appear in the selector
  #    list.
  check [ $(fgrep -c "non_flattened_selector" $FETCH_FILE) -eq 1 ]
  EXPECTED_IMPORT_FAILURE_LINE="<!--Flattening failed: Cannot import http://www.google.com/css/maia.css as it is on an unauthorized domain-->"
  check [ $(grep -o "$EXPECTED_IMPORT_FAILURE_LINE" $FETCH_FILE | wc -l) -eq 1 ]
  EXPECTED_COMMENT_LINE="<!--The preceding resource was not rewritten because its domain (cse.google.com) is not authorized-->"
  check [ $(grep -o "$EXPECTED_COMMENT_LINE" $FETCH_FILE | wc -l) -eq 1 ]

  start_test inline_unauthorized_resources allows unauthorized css selectors
  HOST_NAME="http://unauthorizedresources.example.com"
  URL="$HOST_NAME/mod_pagespeed_test/unauthorized/prioritize_critical_css.html"
  URL+="?PageSpeedFilters=prioritize_critical_css,debug"
  # gsc-completion-selected string should occur once in the html and once in the
  # selector list.  with_unauthorized_imports.css.pagespeed.cf should appear
  # once that file has been optimized.  We need to wait until both of them have
  # been optimized.
  REWRITTEN_UNAUTH_CSS="with_unauthorized_imports\.css\.pagespeed\.cf"
  GSC_SELECTOR="gsc-completion-selected"
  function unauthorized_resources_fully_rewritten() {
    tr '\n' ' ' | \
      grep "$REWRITTEN_UNAUTH_CSS.*$GSC_SELECTOR.*$GSC_SELECTOR" | \
      wc -l
  }
  http_proxy=$SECONDARY_HOSTNAME \
     fetch_until -save $URL unauthorized_resources_fully_rewritten 1
  # Verify that this page had beaconing javascript on it.
  check [ $(fgrep -c "pagespeed.criticalCssBeaconInit" $FETCH_FILE) -eq 3 ]
  # From the css file containing an unauthorized @import line,
  # a) no selectors from the unauthorized @ import (e.g .maia-display) should
  #    appear in the selector list.
  check_not fgrep -q "maia-display" $FETCH_FILE
  # b) no selectors from the authorized @ import (e.g .red) should
  #    appear in the selector list because it won't be flattened.
  check_not fgrep -q "interesting_color" $FETCH_FILE
  # c) selectors that don't depend on flattening should appear in the selector
  #    list.
  check [ $(fgrep -c "non_flattened_selector" $FETCH_FILE) -eq 1 ]
  check_from "$(cat $FETCH_FILE)" grep -q "$EXPECTED_IMPORT_FAILURE_LINE"

  # http://code.google.com/p/modpagespeed/issues/detail?id=494 -- test
  # that fetching a css with embedded relative images from a different
  # VirtualHost, accessing the same content, and rewrite-mapped to the
  # primary domain, delivers results that are cached for a year, which
  # implies the hash matches when serving vs when rewriting from HTML.
  #
  # This rewrites the CSS, absolutifying the embedded relative image URL
  # reference based on the the main server host.
  start_test Relative images embedded in a CSS file served from a mapped domain
  DIR="mod_pagespeed_test/map_css_embedded"
  URL="http://www.example.com/$DIR/issue494.html"
  MAPPED_PREFIX="$DIR/A.styles.css.pagespeed.cf"
  http_proxy=$SECONDARY_HOSTNAME fetch_until $URL \
      "grep -c cdn.example.com/$MAPPED_PREFIX" 1
  MAPPED_CSS=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL | \
      grep -o "$MAPPED_PREFIX..*.css")

  # Now fetch the resource using a different host, which is mapped to the first
  # one.  To get the correct bytes, matching hash, and long TTL, we need to do
  # apply the domain mapping in the CSS resource fetch.
  URL="http://origin.example.com/$MAPPED_CSS"
  echo http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL
  CSS_OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)
  check_from "$CSS_OUT" fgrep -q "Cache-Control: max-age=31536000"

  # Test ForbidFilters, which is set in the config for the VHost
  # forbidden.example.com, where we've forbidden remove_quotes, remove_comments,
  # collapse_whitespace, rewrite_css, and resize_images; we've also disabled
  # inline_css so the link doesn't get inlined since we test that it still has
  # all its quotes.
  FORBIDDEN_TEST_ROOT=http://forbidden.example.com/mod_pagespeed_test
  function test_forbid_filters() {
    QUERYP="$1"
    HEADER="$2"
    URL="$FORBIDDEN_TEST_ROOT/forbidden.html"
    OUTFILE="$TESTTMP/test_forbid_filters"
    echo http_proxy=$SECONDARY_HOSTNAME $WGET $HEADER $URL$QUERYP
    http_proxy=$SECONDARY_HOSTNAME $WGET -q -O $OUTFILE $HEADER $URL$QUERYP
    check egrep -q '<link rel="stylesheet' $OUTFILE
    check egrep -q '<!--'                  $OUTFILE
    check egrep -q '    <li>'              $OUTFILE
    rm -f $OUTFILE
  }
  start_test ForbidFilters baseline check.
  test_forbid_filters "" ""
  start_test ForbidFilters query parameters check.
  QUERYP="?PageSpeedFilters="
  QUERYP="${QUERYP}+remove_quotes,+remove_comments,+collapse_whitespace"
  test_forbid_filters $QUERYP ""
  start_test "ForbidFilters request headers check."
  HEADER="--header=PageSpeedFilters:"
  HEADER="${HEADER}+remove_quotes,+remove_comments,+collapse_whitespace"
  test_forbid_filters "" $HEADER

  start_test ForbidFilters disallows direct resource rewriting.
  FORBIDDEN_EXAMPLE_ROOT=http://forbidden.example.com/mod_pagespeed_example
  FORBIDDEN_STYLES_ROOT=$FORBIDDEN_EXAMPLE_ROOT/styles
  FORBIDDEN_IMAGES_ROOT=$FORBIDDEN_EXAMPLE_ROOT/images
  # .ce. is allowed
  ALLOWED="$FORBIDDEN_STYLES_ROOT/all_styles.css.pagespeed.ce.n7OstQtwiS.css"
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $ALLOWED 2>&1)
  check_200_http_response "$OUT"
  # .cf. is forbidden
  FORBIDDEN=$FORBIDDEN_STYLES_ROOT/A.all_styles.css.pagespeed.cf.UH8L-zY4b4.css
  OUT=$(http_proxy=$SECONDARY_HOSTNAME check_not $WGET -O /dev/null $FORBIDDEN \
    2>&1)
  check_from "$OUT" fgrep -q "404 Not Found"
  # The image will be optimized but NOT resized to the much smaller size,
  # so it will be >200k (optimized) rather than <20k (resized).
  # Use a blocking fetch to force all -allowed- rewriting to be done.
  RESIZED=$FORBIDDEN_IMAGES_ROOT/256x192xPuzzle.jpg.pagespeed.ic.8AB3ykr7Of.jpg
  HEADERS="$OUTDIR/headers"
  http_proxy=$SECONDARY_HOSTNAME $WGET -q --server-response \
    -O $WGET_DIR/forbid.jpg \
    --header 'X-PSA-Blocking-Rewrite: psatest' $RESIZED >& $HEADERS
  LENGTH=$(grep '^ *Content-Length:' $HEADERS | sed -e 's/.*://')
  check test -n "$LENGTH"
  check test $LENGTH -gt 200000
  CCONTROL=$(grep '^ *Cache-Control:' $HEADERS | sed -e 's/.*://')
  check_from "$CCONTROL" grep -w max-age=300
  check_from "$CCONTROL" grep -w private

  start_test Embed image configuration in rewritten image URL.
  # The embedded configuration is placed between the "pagespeed" and "ic", e.g.
  # *xPuzzle.jpg.pagespeed.gp+jp+pj+js+rj+rp+rw+ri+cp+md+iq=73.ic.oFXPiLYMka.jpg
  # We use a regex matching "gp+jp+pj+js+rj+rp+rw+ri+cp+md+iq=73" rather than
  # spelling it out to avoid test regolds when we add image filter IDs.
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save -recursive \
      http://embed-config-html.example.org/embed_config.html \
      'fgrep -c .pagespeed.' 3 --save-headers

  # With the default rewriters in vhost embed-config-resources.example.com
  # the image will be >200k.  But by enabling resizing & compression 73
  # as specified in the HTML domain, and transmitting that configuration via
  # image URL query param, the image file (including headers) is 8341 bytes.
  # We check against 10000 here so this test isn't sensitive to
  # image-compression tweaks (we have enough of those elsewhere).
  check_file_size "$WGET_DIR/256x192xPuz*.pagespeed.*iq=*.ic.*" -lt 10000

  # The CSS file gets rewritten with embedded options, and will have an
  # embedded image in it as well.
  check_file_size \
    "$WGET_DIR/*rewrite_css_images.css.pagespeed.*+ii+*+iq=*.cf.*" -lt 600

  # The JS file is rewritten but has no related options set, so it will
  # not get the embedded options between "pagespeed" and "jm".
  check_file_size "$WGET_DIR/rewrite_javascript.js.pagespeed.jm.*.js" -lt 500

  # Count how many bytes there are of body, skipping the initial headers.
  function body_size {
    fname="$1"
    tail -n+$(($(extract_headers $fname | wc -l) + 1)) $fname | wc -c
  }

  # One flaw in the above test is that it short-circuits the decoding
  # of the query-params because when pagespeed responds to the recursive
  # wget fetch of the image, it finds the rewritten resource in the
  # cache.  The two vhosts are set up with the same cache.  If they
  # had different caches we'd have a different problem, which is that
  # the first load of the image-rewrite from the resource vhost would
  # not be resized.  To make sure the decoding path works, we'll
  # "finish" this test below after performing a cache flush, saving
  # the encoded image and expected size.
  EMBED_CONFIGURATION_IMAGE="http://embed-config-resources.example.com/images/"
  EMBED_CONFIGURATION_IMAGE_TAIL=$(ls $WGET_DIR | grep 256x192xPuz | grep iq=)
  EMBED_CONFIGURATION_IMAGE+="$EMBED_CONFIGURATION_IMAGE_TAIL"
  EMBED_CONFIGURATION_IMAGE_LENGTH=$(
    body_size "$WGET_DIR/$EMBED_CONFIGURATION_IMAGE_TAIL")

  # Grab the URL for the CSS file.
  EMBED_CONFIGURATION_CSS_LEAF=$(ls $WGET_DIR | \
      grep '\.pagespeed\..*+ii+.*+iq=.*\.cf\..*')
  EMBED_CONFIGURATION_CSS_LENGTH=$(
    body_size $WGET_DIR/$EMBED_CONFIGURATION_CSS_LEAF)

  EMBED_CONFIGURATION_CSS_URL="http://embed-config-resources.example.com/styles"
  EMBED_CONFIGURATION_CSS_URL+="/$EMBED_CONFIGURATION_CSS_LEAF"

  # Grab the URL for that embedded image; it should *also* have the embedded
  # configuration options in it, though wget/recursive will not have pulled
  # it to a file for us (wget does not parse CSS) so we'll have to request it.
  EMBED_CONFIGURATION_CSS_IMAGE=$WGET_DIR/*images.css.pagespeed.*+ii+*+iq=*.cf.*
  EMBED_CONFIGURATION_CSS_IMAGE_URL=$(egrep -o \
    'http://.*iq=[0-9]*\.ic\..*\.jpg' \
    $EMBED_CONFIGURATION_CSS_IMAGE)
  # fetch that file and make sure it has the right cache-control
  http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP \
     $EMBED_CONFIGURATION_CSS_IMAGE_URL > "$WGET_DIR/img"
  CSS_IMAGE_HEADERS=$(head -10 "$WGET_DIR/img")
  check_from "$CSS_IMAGE_HEADERS" fgrep -q "Cache-Control: max-age=31536000"
  EMBED_CONFIGURATION_CSS_IMAGE_LENGTH=$(body_size "$WGET_DIR/img")

  function embed_image_config_post_flush() {
    # Finish off the url-params-.pagespeed.-resource tests with a clear
    # cache.  We split the test like this to avoid having multiple
    # places where we flush cache, which requires sleeps since the
    # cache-flush is poll driven.
    start_test Embed image/css configuration decoding with clear cache.
    echo Looking for $EMBED_CONFIGURATION_IMAGE expecting \
        $EMBED_CONFIGURATION_IMAGE_LENGTH bytes
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$EMBED_CONFIGURATION_IMAGE" \
        "wc -c" $EMBED_CONFIGURATION_IMAGE_LENGTH

    echo Looking for $EMBED_CONFIGURATION_CSS_IMAGE_URL expecting \
        $EMBED_CONFIGURATION_CSS_IMAGE_LENGTH bytes
    http_proxy=$SECONDARY_HOSTNAME fetch_until \
        "$EMBED_CONFIGURATION_CSS_IMAGE_URL" \
        "wc -c" $EMBED_CONFIGURATION_CSS_IMAGE_LENGTH

    echo Looking for $EMBED_CONFIGURATION_CSS_URL expecting \
        $EMBED_CONFIGURATION_CSS_LENGTH bytes
    http_proxy=$SECONDARY_HOSTNAME fetch_until \
        "$EMBED_CONFIGURATION_CSS_URL" \
        "wc -c" $EMBED_CONFIGURATION_CSS_LENGTH
  }
  on_cache_flush embed_image_config_post_flush

  # Make sure that when in PreserveURLs mode that we don't rewrite URLs. This is
  # non-exhaustive, the unit tests should cover the rest.  Note: We block with
  # psatest here because this is a negative test.  We wouldn't otherwise know
  # how many wget attempts should be made.
  start_test PreserveURLs on prevents URL rewriting
  WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
  WGET_ARGS+=" --header=Host:preserveurls.example.com"

  FILE=preserveurls/on/preserveurls.html
  URL=$SECONDARY_HOSTNAME/mod_pagespeed_test/$FILE
  FETCHED=$OUTDIR/preserveurls.html
  check run_wget_with_args $URL
  check_not fgrep -q .pagespeed. $FETCHED

  # When PreserveURLs is off do a quick check to make sure that normal rewriting
  # occurs.  This is not exhaustive, the unit tests should cover the rest.
  start_test PreserveURLs off causes URL rewriting
  WGET_ARGS="--header=Host:preserveurls.example.com"
  FILE=preserveurls/off/preserveurls.html
  URL=$SECONDARY_HOSTNAME/mod_pagespeed_test/$FILE
  FETCHED=$OUTDIR/preserveurls.html
  # Check that style.css was inlined.
  fetch_until $URL 'egrep -c big.css.pagespeed.' 1
  # Check that introspection.js was inlined.
  fetch_until $URL 'grep -c document\.write(\"External' 1
  # Check that the image was optimized.
  fetch_until $URL 'grep -c BikeCrashIcn\.png\.pagespeed\.' 1

  # TODO(jkarlin): When ajax rewriting is in system/ check that it works here.

  # When Cache-Control: no-transform is in the response make sure that
  # the URL is not rewritten and that the no-transform header remains
  # in the resource.
  start_test HonorNoTransform cache-control: no-transform
  WGET_ARGS="--header=X-PSA-Blocking-Rewrite:psatest"
  FILE=no_transform/image.html
  URL=$TEST_ROOT/$FILE
  FETCHED=$OUTDIR/output
  wget -O - $URL $WGET_ARGS > $FETCHED
  # Make sure that the URL is not rewritten
  check_not fgrep -q '.pagespeed.' $FETCHED
  wget -O - -S $TEST_ROOT/no_transform/BikeCrashIcn.png $WGET_ARGS &> $FETCHED
  # Make sure that the no-transfrom header is still there
  check grep -q 'Cache-Control:.*no-transform' $FETCHED

  # If DisableRewriteOnNoTransform is turned off, verify that the rewriting
  # applies even if Cache-control: no-transform is set.
  start_test rewrite on Cache-control: no-transform
  URL=$TEST_ROOT/disable_no_transform/index.html?PageSpeedFilters=inline_css
  fetch_until -save -recursive $URL 'grep -c style' 2

  # TODO(jkarlin): Now that IPRO is implemented we should test that we obey
  # no-transform in that path.

  start_test respect vary user-agent
  URL="$SECONDARY_HOSTNAME/mod_pagespeed_test/vary/index.html"
  URL+="?PageSpeedFilters=inline_css"
  FETCH_CMD="$WGET_DUMP --header=Host:respectvary.example.com $URL"
  OUT=$($FETCH_CMD)
  # We want to verify that css is not inlined, but if we just check once then
  # pagespeed doesn't have long enough to be able to inline it.
  sleep .1
  OUT=$($FETCH_CMD)
  check_not_from "$OUT" fgrep "<style>"

  test_filter inline_javascript inlines a small JS file
  start_test no inlining of unauthorized resources
  URL="$TEST_ROOT/unauthorized/inline_unauthorized_javascript.html?"
  URL+="PageSpeedFilters=inline_javascript,debug"
  OUTFILE=$OUTDIR/blocking_rewrite.out.html
  $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $OUTFILE
  check egrep -q 'script[[:space:]]src=' $OUTFILE
  EXPECTED_COMMENT_LINE="<!--The preceding resource was not rewritten because"
  EXPECTED_COMMENT_LINE+=" its domain (www.gstatic.com) is not authorized-->"
  check [ $(grep -o "$EXPECTED_COMMENT_LINE" $OUTFILE | wc -l) -eq 1 ]

  start_test inline_unauthorized_resources allows inlining
  HOST_NAME="http://unauthorizedresources.example.com"
  URL="$HOST_NAME/mod_pagespeed_test/unauthorized/"
  URL+="inline_unauthorized_javascript.html?PageSpeedFilters=inline_javascript"
  http_proxy=$SECONDARY_HOSTNAME \
      fetch_until $URL 'grep -c script[[:space:]]src=' 0

  start_test inline_unauthorized_resources does not allow rewriting
  URL="$HOST_NAME/mod_pagespeed_test/unauthorized/"
  URL+="inline_unauthorized_javascript.html?PageSpeedFilters=rewrite_javascript"
  OUTFILE=$OUTDIR/blocking_rewrite.out.html
  http_proxy=$SECONDARY_HOSTNAME \
      $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $OUTFILE
  check egrep -q 'script[[:space:]]src=' $OUTFILE

  # Verify that we can control pagespeed settings via a response
  # header passed from an origin to a reverse proxy.
  start_test Honor response header direcives from origin
  URL="http://rproxy.rmcomments.example.com/"
  URL+="mod_pagespeed_example/remove_comments.html"
  echo http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL ...
  OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL)
  check_from "$OUT" fgrep -q "remove_comments example"
  check_not_from "$OUT" fgrep -q "This comment will be removed"

  test_filter inline_css inlines a small CSS file
  start_test no inlining of unauthorized resources
  URL="$TEST_ROOT/unauthorized/inline_css.html?"
  URL+="PageSpeedFilters=inline_css,debug"
  OUTFILE=$OUTDIR/blocking_rewrite.out.html
  $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $OUTFILE
  check egrep -q 'link[[:space:]]rel=' $OUTFILE
  EXPECTED_COMMENT_LINE="<!--The preceding resource was not rewritten because"
  EXPECTED_COMMENT_LINE+=" its domain (cse.google.com) is not authorized-->"
  check [ $(grep -o "$EXPECTED_COMMENT_LINE" $OUTFILE | wc -l) -eq 1 ]

  start_test inline_unauthorized_resources allows inlining
  HOST_NAME="http://unauthorizedresources.example.com"
  URL="$HOST_NAME/mod_pagespeed_test/unauthorized/"
  URL+="inline_css.html?PageSpeedFilters=inline_css"
  http_proxy=$SECONDARY_HOSTNAME \
      fetch_until $URL 'grep -c link[[:space:]]rel=' 0

  start_test inline_unauthorized_resources does not allow rewriting
  URL="$HOST_NAME/mod_pagespeed_test/unauthorized/"
  URL+="inline_css.html?PageSpeedFilters=rewrite_css"
  OUTFILE=$OUTDIR/blocking_rewrite.out.html
  http_proxy=$SECONDARY_HOSTNAME \
      $WGET_DUMP --header 'X-PSA-Blocking-Rewrite: psatest' $URL > $OUTFILE
  check egrep -q 'link[[:space:]]rel=' $OUTFILE

  # TODO(sligocki): Following test only works with query_params. Fix to work
  # with any method and get rid of this manual set.
  filter_spec_method="query_params"
  # Test for MaxCombinedCssBytes. The html used in the test, 'combine_css.html',
  # has 4 CSS files in the following order.
  #   yellow.css :   36 bytes
  #   blue.css   :   21 bytes
  #   big.css    : 4307 bytes
  #   bold.css   :   31 bytes
  # Because the threshold was chosen as '57', only the first two CSS files
  # are combined.
  test_filter combine_css Maximum size of combined CSS.
  QUERY_PARAM="PageSpeedMaxCombinedCssBytes=57"
  URL="$URL&$QUERY_PARAM"
  # Make sure that we have exactly 3 CSS files (after combination).
  fetch_until -save $URL 'grep -c text/css' 3
  # Now check that the 1st and 2nd CSS files are combined, but the 3rd
  # one is not.
  check [ $(grep -c 'styles/yellow.css+blue.css.pagespeed.' \
      $FETCH_UNTIL_OUTFILE) = 1 ]
  check [ $(grep -c 'styles/big.css\"' $FETCH_UNTIL_OUTFILE) = 1 ]
  check [ $(grep -c 'styles/bold.css\"' $FETCH_UNTIL_OUTFILE) = 1 ]

  # Test to make sure we have a sane Connection Header.  See
  # https://code.google.com/p/modpagespeed/issues/detail?id=664
  #
  # Note that this bug is dependent on seeing a resource for the first time in
  # the InPlaceResourceOptimization path, because in that flow we are caching
  # the response-headers from the server.  The reponse-headers from Serf never
  # seem to include the Connection header.  So we have to cachebust the JS file.
  start_test Sane Connection header
  URL="$TEST_ROOT/normal.js?q=cachebust"
  fetch_until -save $URL 'grep -c W/\"PSA-aj-' 1 --save-headers
  CONNECTION=$(extract_headers $FETCH_UNTIL_OUTFILE | fgrep "Connection:")
  check_not_from "$CONNECTION" fgrep -qi "Keep-Alive, Keep-Alive"
  check_from "$CONNECTION" fgrep -qi "Keep-Alive"

  start_test Handler access restrictions
  function expect_handler() {
    local host_prefix="$1"
    local handler="$2"
    local expectation="$3"

    URL="http://$host_prefix.example.com/$handler"
    echo "http_proxy=$SECONDARY_HOSTNAME curl $URL"
    OUT=$(http_proxy=$SECONDARY_HOSTNAME \
      $CURL -o/dev/null -sS --write-out '%{http_code}\n' "$URL")
    if [ "$expectation" = "allow" ]; then
      check [ "$OUT" = "200" ]
    elif [ "$expectation" = "deny" ]; then
      check [ "$OUT" = "403" -o "$OUT" = "404" ]
    else
      check false
    fi
  }
  function expect_messages() {
    expect_handler "$1" "$MESSAGES_HANDLER" "$2"
  }

  # Listed at top level.
  expect_messages messages-allowed allow
  # Listed at top level.
  expect_messages more-messages-allowed allow
  # Not listed at any level.
  expect_messages messages-still-not-allowed deny
  # Listed at VHost level.
  expect_messages but-this-message-allowed allow
  # Listed at VHost level.
  expect_messages and-this-one allow
  # Listed at top level, VHost level lists CLEAR_INHERITED.
  expect_messages cleared-inherited deny
  # Listed at top level, VHost level lists both this and CLEAR_INHERITED.
  expect_messages cleared-inherited-reallowed allow
  # Not listed at top level, VHost level lists both this and CLEAR_INHERITED.
  expect_messages messages-allowed-at-vhost allow
  # Not listed at any level, VHost level lists only CLEAR_INHERITED.
  expect_messages cleared-inherited-unlisted allow
  # Not listed at any level, VHost level lists CLEAR_INHERITED and some other
  # domains.
  expect_messages messages-not-allowed-at-vhost deny
  # Listed at top level, via wildcard.
  expect_messages anything-a-wildcard allow
  # Listed at top level, via wildcard.
  expect_messages anything-b-wildcard allow
  # Listed at top level, via wildcard, VHost level lists CLEAR_INHERITED.
  expect_messages anything-c-wildcard deny
  # VHost lists deny *
  expect_messages nothing-allowed deny

  if [ "$NO_VHOST_MERGE" = "on" ]; then
    # In Apache the global config may or may not be inherited into the vhost
    # config.  This affects option merging here, so for some tests we need to
    # sets of expectations.  If NO_VHOST_MERGE is set, that's equivalent here to
    # CLEAR_INHERITED.

    # Not listed at any level.
    expect_messages messages-not-allowed allow
  else
    # Not listed at any level.
    expect_messages messages-not-allowed deny
    # Listed at top level, VHost level lists CLEAR_INHERITED.
    expect_messages cleared-inherited deny
  fi

  # No <Handler>Domains listings for these, default is allow.
  expect_handler nothing-explicitly-allowed $STATISTICS_HANDLER allow
  expect_handler nothing-explicitly-allowed $GLOBAL_STATISTICS_HANDLER allow
  expect_handler nothing-explicitly-allowed pagespeed_admin/ allow
  expect_handler nothing-explicitly-allowed pagespeed_global_admin/ allow
  expect_handler nothing-explicitly-allowed pagespeed_console allow

  # Listed at VHost level as allowed.
  expect_handler everything-explicitly-allowed $STATISTICS_HANDLER allow
  expect_handler everything-explicitly-allowed $GLOBAL_STATISTICS_HANDLER allow
  expect_handler everything-explicitly-allowed pagespeed_admin/ allow
  expect_handler everything-explicitly-allowed pagespeed_global_admin/ allow
  expect_handler everything-explicitly-allowed pagespeed_console allow

  # Other domains listed at VHost level as allowed, none of these listed.
  expect_handler everything-explicitly-allowed-but-aliased \
    $STATISTICS_HANDLER deny
  expect_handler everything-explicitly-allowed-but-aliased \
    $GLOBAL_STATISTICS_HANDLER deny
  expect_handler everything-explicitly-allowed-but-aliased \
    pagespeed_admin/ deny
  expect_handler everything-explicitly-allowed-but-aliased \
    pagespeed_global_admin/ deny
  expect_handler everything-explicitly-allowed-but-aliased \
    pagespeed_console deny
fi

start_test ShowCache without URL gets a form, inputs, preloaded UA.
ADMIN_CACHE=$PRIMARY_SERVER/pagespeed_admin/cache
OUT=$($WGET_DUMP $ADMIN_CACHE)
check_from "$OUT" fgrep -q "<form>"
check_from "$OUT" fgrep -q "<input "
check_from "$OUT" fgrep -q "Cache-Control: max-age=0, no-cache"
# Preloaded user_agent value field leading with "Mozilla" set in
# ../automatic/system_test_helpers.sh to help test a "normal" flow.
check_from "$OUT" fgrep -q 'name=user_agent value="Mozilla'

start_test ShowCache with bogus URL gives a 404
OUT=$(check_error_code 8 $WGET -O - --save-headers \
  $PRIMARY_SERVER/pagespeed_cache?url=bogus_format 2>&1)
check_from "$OUT" fgrep -q "ERROR 404: Not Found"

start_test ShowCache with valid, present URL, with unique options.
options="PageSpeedImageInlineMaxBytes=6765"
fetch_until -save $EXAMPLE_ROOT/rewrite_images.html?$options \
    'grep -c Puzzle\.jpg\.pagespeed\.ic\.' 1
URL_TAIL=$(grep Puzzle $FETCH_UNTIL_OUTFILE | cut -d \" -f 2)
SHOW_CACHE_URL=$EXAMPLE_ROOT/$URL_TAIL
SHOW_CACHE_QUERY=$ADMIN_CACHE?url=$SHOW_CACHE_URL\&$options
echo "$WGET_DUMP $SHOW_CACHE_QUERY"
OUT=$($WGET_DUMP $SHOW_CACHE_QUERY)

# TODO(jmarantz): I have seen this test fail in the 'apache_debug_leak_test'
# phase of checkin tests.  The failure is that we are able to rewrite the
# HTML image link for Puzzle.jpg, but when fetching the metadata cache entry
# it comes back as cache_ok:false.
#
# This should be investigated, especially if it happens again.  Wild
# guess: the shared-memory metadata cache might face an unexpected
# eviction as it is not a pure LRU.
#
# Another guess: I think we might be calling callbacks with updated
# metadata cache before writing the metadata cache entries to the cache
# in some flows, e.g. in ResourceReconstructCallback::HandleDone.  It's not
# obvious why that flow would affect this test, but the ordering
# of writing cache and calling callbacks deserves an audit.
check_from "$OUT" fgrep -q cache_ok:true
check_from "$OUT" fgrep -q mod_pagespeed_example/images/Puzzle.jpg

function show_cache_after_flush() {
  start_test ShowCache with same URL and matching options misses after flush
  OUT=$($WGET_DUMP $SHOW_CACHE_QUERY)
  check_from "$OUT" fgrep -q cache_ok:false
}

on_cache_flush show_cache_after_flush

start_test ShowCache with same URL but new options misses.
options="PageSpeedImageInlineMaxBytes=6766"
OUT=$($WGET_DUMP $ADMIN_CACHE?url=$SHOW_CACHE_URL\&$options)
check_from "$OUT" fgrep -q cache_ok:false

# Test if the warning messages are colored in message_history page.
# We color the messages in message_history page to make it clearer to read.
# Red for Error messages. Brown for Warning messages.
# Orange for Fatal messages. Black by default.
# Won't test Error messages and Fatal messages in this test.
start_test Messages are colored in message_history
INJECT=$($CURL --silent $HOSTNAME/?PageSpeed=Warning_trigger)
OUT=$($WGET -q -O - $HOSTNAME/pagespeed_admin/message_history | \
  grep Warning_trigger)
check_from "$OUT" fgrep -q "color:brown;"

# Test handling of large HTML files. We first test with a cold cache, and verify
# that we bail out of parsing and insert a script redirecting to
# ?PageSpeed=off. This should also insert an entry into the property cache so
# that the next time we fetch the file it will not be parsed at all.
echo TEST: Handling of large files.
# Add a timestamp to the URL to ensure it's not in the property cache.
FILE="max_html_parse_size/large_file.html?value=$(date +%s)"
URL=$TEST_ROOT/$FILE
# Enable a filter that will modify something on this page, since we testing that
# this page should not be rewritten.
WGET_EC="$WGET_DUMP --header=PageSpeedFilters:rewrite_images"
echo $WGET_EC $URL
LARGE_OUT=$($WGET_EC $URL)
check_from "$LARGE_OUT" grep -q window.location=".*&PageSpeed=off"

# The file should now be in the property cache so make sure that the page is no
# longer parsed. Use fetch_until because we need to wait for a potentially
# non-blocking write to the property cache from the previous test to finish
# before this will succeed.
fetch_until -save $URL 'grep -c window.location=".*&PageSpeed=off"' 0
check_not fgrep -q pagespeed.ic $FETCH_FILE

start_test IPRO-optimized resources should have fixed size, not chunked.
URL="$EXAMPLE_ROOT/images/Puzzle.jpg"
URL+="?PageSpeedJpegRecompressionQuality=75"
fetch_until -save $URL "wc -c" 90000 "--save-headers" "-lt"
check_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" grep -q '^Content-Length:'
CONTENT_LENGTH=$(extract_headers $FETCH_UNTIL_OUTFILE | \
  grep '^Content-Length:' | awk '{print $2}')
check [ "$CONTENT_LENGTH" -lt 90000 ];
check_not_from "$(extract_headers $FETCH_UNTIL_OUTFILE)" \
    fgrep -q 'Transfer-Encoding: chunked'

start_test IPRO 304 with etags
# Reuses $URL and $FETCH_UNTIL_OUTFILE from previous test.
case_normalized_headers=$(
  extract_headers $FETCH_UNTIL_OUTFILE | sed 's/^Etag:/ETag:/')
check_from "$case_normalized_headers" fgrep -q 'ETag:'
ETAG=$(echo "$case_normalized_headers" | awk '/ETag:/ {print $2}')
echo $WGET_DUMP --header "If-None-Match: $ETAG" $URL
OUTFILE=$OUTDIR/etags
# Note: -o gets debug info which is the only place that 304 message is sent.
check_not $WGET -o $OUTFILE -O /dev/null --header "If-None-Match: $ETAG" $URL
check fgrep -q "awaiting response... 304" $OUTFILE

start_test Accept bad query params and headers

# The examples page should have this EXPECTED_EXAMPLES_TEXT on it.
EXPECTED_EXAMPLES_TEXT="PageSpeed Examples Directory"
OUT=$(wget -O - $EXAMPLE_ROOT)
check_from "$OUT" fgrep -q "$EXPECTED_EXAMPLES_TEXT"

# It should still be there with bad query params.
OUT=$(wget -O - $EXAMPLE_ROOT?PageSpeedFilters=bogus)
check_from "$OUT" fgrep -q "$EXPECTED_EXAMPLES_TEXT"

# And also with bad request headers.
OUT=$(wget -O - --header=PageSpeedFilters:bogus $EXAMPLE_ROOT)
check_from "$OUT" fgrep -q "$EXPECTED_EXAMPLES_TEXT"

# Tests that an origin header with a Vary header other than Vary:Accept-Encoding
# loses that header when we are not respecting vary.
start_test Vary:User-Agent on resources is held by our cache.
URL="$TEST_ROOT/vary/no_respect/index.html"
fetch_until -save $URL 'fgrep -c .pagespeed.cf.' 1

# Extract out the rewritten CSS file from the HTML saved by fetch_until
# above (see -save and definition of fetch_until).  Fetch that CSS
# file with headers and make sure the Vary is stripped.
CSS_URL=$(grep stylesheet $FETCH_UNTIL_OUTFILE | cut -d\" -f 4)
CSS_URL="$TEST_ROOT/vary/no_respect/$(basename $CSS_URL)"
echo CSS_URL=$CSS_URL
CSS_OUT=$($WGET_DUMP $CSS_URL)
check_from "$CSS_OUT" fgrep -q "Vary: Accept-Encoding"
check_not_from "$CSS_OUT" fgrep -q "User-Agent"



start_test UseExperimentalJsMinifier
URL="$TEST_ROOT/experimental_js_minifier/index.html"
URL+="?PageSpeedFilters=rewrite_javascript"
# External scripts rewritten.
fetch_until -save -recursive $URL 'grep -c src=.*\.pagespeed\.jm\.' 1
check_not grep "removed" $WGET_DIR/*   # No comments should remain.
check grep -q "preserved" $WGET_DIR/*  # Contents of <script src=> element kept.
ORIGINAL_HTML_SIZE=1484
check_file_size $FETCH_FILE -lt $ORIGINAL_HTML_SIZE  # Net savings
# Rewritten JS is cache-extended.
check grep -qi "Cache-control: max-age=31536000" $WGET_OUTPUT
check grep -qi "Expires:" $WGET_OUTPUT

start_test Source map tests
URL="$TEST_ROOT/experimental_js_minifier/index.html"
URL+="?PageSpeedFilters=rewrite_javascript,include_js_source_maps"
# All rewriting still happening as expected.
fetch_until -save -recursive $URL 'grep -c src=.*\.pagespeed\.jm\.' 1
check_not grep "removed" $WGET_DIR/*  # No comments should remain.
check_file_size $FETCH_FILE -lt $ORIGINAL_HTML_SIZE  # Net savings
check grep -qi "Cache-control: max-age=31536000" $WGET_OUTPUT
check grep -qi "Expires:" $WGET_OUTPUT

# No source map for inline JS
check_not grep sourceMappingURL $FETCH_FILE
# Yes source_map for external JS
check grep -q sourceMappingURL $WGET_DIR/script.js.pagespeed.*
SOURCE_MAP_URL=$(grep sourceMappingURL $WGET_DIR/script.js.pagespeed.* |
                 grep -o 'http://.*')
OUTFILE=$OUTDIR/source_map
check $WGET_DUMP -O $OUTFILE $SOURCE_MAP_URL
check grep -qi "Cache-control: max-age=31536000" $OUTFILE  # Long cache
check grep -q "script.js?PageSpeed=off" $OUTFILE  # Has source URL.
check grep -q '"mappings":' $OUTFILE  # Has mappings.

test_filter combine_css combines 4 CSS files into 1.
fetch_until $URL 'grep -c text/css' 1
check run_wget_with_args $URL
test_resource_ext_corruption $URL $combine_css_filename

test_filter extend_cache rewrites an image tag.
fetch_until $URL 'grep -c src.*91_WewrLtP' 1
check run_wget_with_args $URL
echo about to test resource ext corruption...
test_resource_ext_corruption $URL images/Puzzle.jpg.pagespeed.ce.91_WewrLtP.jpg

test_filter outline_javascript outlines large scripts, but not small ones.
check run_wget_with_args $URL
check egrep -q '<script.*large.*src=' $FETCHED       # outlined
check egrep -q '<script.*small.*var hello' $FETCHED  # not outlined
start_test compression is enabled for rewritten JS.
JS_URL=$(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED)
echo "JS_URL=\$\(egrep -o http://.*[.]pagespeed.*[.]js $FETCHED\)=\"$JS_URL\""
JS_HEADERS=$($WGET -O /dev/null -q -S --header='Accept-Encoding: gzip' \
  $JS_URL 2>&1)
echo JS_HEADERS=$JS_HEADERS
check_200_http_response "$JS_HEADERS"
check_from "$JS_HEADERS" fgrep -qi 'Content-Encoding: gzip'
check_from "$JS_HEADERS" fgrep -qi 'Vary: Accept-Encoding'
check_from "$JS_HEADERS" egrep -qi '(Etag: W/"0")|(Etag: W/"0-gzip")'
check_from "$JS_HEADERS" fgrep -qi 'Last-Modified:'

# Test RetainComment directive.
test_filter remove_comments retains appropriate comments.
check run_wget_with_args $URL
check fgrep -q retained $FETCHED        # RetainComment directive

start_test IPRO source map tests
URL="$TEST_ROOT/experimental_js_minifier/script.js"
URL+="?PageSpeedFilters=rewrite_javascript,include_js_source_maps"
# Fetch until IPRO removes comments.
fetch_until -save $URL 'grep -c removed' 0
# Yes source_map for external JS
check grep -q sourceMappingURL $FETCH_FILE
SOURCE_MAP_URL=$(grep sourceMappingURL $FETCH_FILE | grep -o 'http://.*')
OUTFILE=$OUTDIR/source_map
check $WGET_DUMP -O $OUTFILE $SOURCE_MAP_URL
check grep -qi "Cache-control: max-age=31536000" $OUTFILE  # Long cache
check grep -q "script.js?PageSpeed=off" $OUTFILE  # Has source URL.
check grep -q '"mappings":' $OUTFILE  # Has mappings.

function cache_purge_test() {
  # Tests for individual URL purging, and for global cache purging via
  # GET pagespeed_admin/cache?purge=URL, and PURGE URL methods.
  PURGE_ROOT="$1"
  PURGE_STATS_URL="$PURGE_ROOT/pagespeed_admin/statistics"
  function cache_purge() {
    local purge_method="$1"
    local purge_path="$2"
    if [ "$purge_method" = "GET" ]; then
      echo http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          "$PURGE_ROOT/pagespeed_admin/cache?purge=$purge_path"
      http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          "$PURGE_ROOT/pagespeed_admin/cache?purge=$purge_path"
    else
      PURGE_URL="$PURGE_ROOT/$purge_path"
      echo $CURL --request PURGE --proxy $SECONDARY_HOSTNAME "$PURGE_URL"
      check $CURL --request PURGE --proxy $SECONDARY_HOSTNAME "$PURGE_URL"
    fi
    echo ""
    if [ $statistics_enabled -eq "0" ]; then
      # Without statistics, we have no mechanism to transmit state-changes
      # from one Apache child process to another, and so each process must
      # independently poll the cache.purge file, which happens every 5 seconds.
      echo sleep 6
      sleep 6
    fi
  }

  # Checks to see whether a .pagespeed URL is present in the metadata cache.
  # A response including "cache_ok:true" or "cache_ok:false" is send to stdout.
  function read_metadata_cache() {
    path="$PURGE_ROOT/$1"
    http_proxy=$SECONDARY_HOSTNAME $WGET -q -O - \
          "$PURGE_ROOT/pagespeed_admin/cache?url=$path"
  }

  # Find the full .pagespeed. URL of yellow.css
  PURGE_COMBINE_CSS="$PURGE_ROOT/combine_css.html"
  http_proxy=$SECONDARY_HOSTNAME fetch_until -save "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
  yellow_css=$(grep yellow.css $FETCH_UNTIL_OUTFILE | cut -d\" -f6)
  blue_css=$(grep blue.css $FETCH_UNTIL_OUTFILE | cut -d\" -f6)

  purple_path="styles/$$"
  purple_url="$PURGE_ROOT/$purple_path/purple.css"
  purple_dir="$APACHE_DOC_ROOT/purge/$purple_path"
  ls -ld $APACHE_DOC_ROOT $APACHE_DOC_ROOT/purge
  echo $SUDO mkdir -p "$purple_dir"
  $SUDO mkdir -p "$purple_dir"
  purple_file="$purple_dir/purple.css"

  for method in $CACHE_PURGE_METHODS; do
    echo Individual URL Cache Purging with $method
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:true
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:true
    echo 'body { background: MediumPurple; }' > "/tmp/purple.$$"
    $SUDO cp "/tmp/purple.$$" "$purple_file"
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$purple_url" 'fgrep -c 9370db' 1
    echo 'body { background: black; }' > "/tmp/purple.$$"
    $SUDO cp "/tmp/purple.$$" "$purple_file"

    cache_purge $method "*"

    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:false
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:false
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$purple_url" 'fgrep -c #000' 1
    cache_purge "$method" "$purple_path/purple.css"

    sleep 1
    STATS=$OUTDIR/purge.stats
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.0
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.1

    # Having rewritten 4 CSS files, we will have done 4 resources fetches.
    check_stat $STATS.0 $STATS.1 num_resource_fetch_successes 4

    # Sanity check: rewriting the same CSS file results in no new fetches.
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.2
    check_stat $STATS.1 $STATS.2 num_resource_fetch_successes 0

    # Now flush one of the files, and it should be the only one that
    # needs to be refetched after we get the combine_css file again.
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:true
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:true
    cache_purge $method styles/yellow.css
    check_from "$(read_metadata_cache $yellow_css)" fgrep -q cache_ok:false
    check_from "$(read_metadata_cache $blue_css)" fgrep -q cache_ok:true

    sleep 1
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$PURGE_COMBINE_CSS" \
      "grep -c pagespeed.cf" 4
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $PURGE_STATS_URL > $STATS.3
    check_stat $STATS.2 $STATS.3 num_resource_fetch_successes 1
  done
  $SUDO rm -rf "$purple_dir" "/tmp/purple.$$"
}

if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  start_test Cache purge test
  cache_purge_test http://purge.example.com

  # Run a simple cache_purge test but in a vhost with ModPagespeed off, and
  # a subdirectory with htaccess file turning it back on, addressing
  # https://github.com/pagespeed/mod_pagespeed/issues/1077
  #
  # TODO(jefftk): Uncomment this and delete uncomment the same test in
  # apache/system_test.sh once nginx_system_test suppressions &/or
  # "pagespeed off;" in server block allow location-overrides in ngx_pagespeed.
  # See https://github.com/pagespeed/ngx_pagespeed/issues/968
  # start_test Cache purging with PageSpeed off in vhost, but on in directory.
  # cache_purge_test http://psoff-dir-on.example.com
fi

start_test Ipro transcode to webp, iterating with Noop
# There's a trick for making demo pages that show the fully optimized ipro
# images without relying on the user flushing the browser cache, which relies
# on a 'Noop' option setting via query-param.  The Noop option does not enter
# into signature computation, but does get stripped from HTTP cache keys.  To
# test this, we must first fetch an image with ?PageSpeedNoop=RANDOM1, until
# it is optimized.  Then we fetch the same image with ?PageSpeedNoop=RANDOM2,
# and we expect there will be no extra image optimizations.
#
# To use this trick you fetch the same URL from JavaScript until you get
# the optimized result, bumping the PageSpeedNoop version each time in the
# query param, which busts the browser cache but not the pagespeed Metadata
# or HTTP cache.  The Metadata cache does not bust because "Noop" is excluded
# from signature computation.  The HTTP cache is not busted because the
# pagespeed query params are stripped from the generate internal .pagespeed.
# URL (but not other query params).
#
# As we are checking some statistics, try get the system to quiesce to reduce
# flakiness from outstanding background rewrites triggered by tests above.
echo -n Waiting for quiescence by checking serf_fetch_active_count ...
while [ $(scrape_stat serf_fetch_active_count) -gt 0 ]; do
  echo -n .
  sleep .1
done
sleep 2
echo " done"
URL="$EXAMPLE_ROOT/images/Puzzle.jpg"
URL+="?PageSpeedFilters=+in_place_optimize_for_browser"
WGET_ARGS="--user-agent webp --header Accept:image/webp"
RANDOM1=$RANDOM
RANDOM2=$((RANDOM1 + 1))
URL1="${URL}&PageSpeedNoop=$RANDOM1"
URL2="${URL}&PageSpeedNoop=$RANDOM2"
fetch_until "$URL1" "grep -c image/webp" 1 --save-headers
#NUM_REWRITES_URL1=$(scrape_stat image_rewrites)
NUM_FETCHES_URL1=$(scrape_stat http_fetches)
check $WGET -q $WGET_ARGS --save-headers "$URL2" -O $WGET_OUTPUT
#NUM_REWRITES_URL2=$(scrape_stat image_rewrites)
NUM_FETCHES_URL2=$(scrape_stat http_fetches)
check_from "$(extract_headers $WGET_OUTPUT)" grep -q "image/webp"
#check [ $NUM_REWRITES_URL2 = $NUM_REWRITES_URL1 ]
check [ $NUM_FETCHES_URL2 = $NUM_FETCHES_URL1 ]
URL=""

if [ "$CACHE_FLUSH_TEST" = "on" ]; then
  start_test 2-pass ipro with long ModPagespeedInPlaceRewriteDeadline
  cmd="$WGET_DUMP $TEST_ROOT/ipro/wait/long/purple.css"
  echo $cmd; OUT=$($cmd)
  echo first pass: the fetch  must occur first regardless of the deadline.
  check_from "$OUT" fgrep -q 'background: MediumPurple;'
  echo second pass: a long deadline and an easy optimization
  echo make an optimized result very likely on the second pass.
  echo $cmd; OUT=$($cmd)
  check_from "$OUT" fgrep -q 'body{background:#9370db}'

  start_test 3-pass ipro with short ModPagespeedInPlaceRewriteDeadline
  cmd="$WGET_DUMP $TEST_ROOT/ipro/wait/short/Puzzle.jpg "
  echo $cmd; bytes=$($cmd | wc -c)
  echo first pass: the fetch  must occur first regardless of the deadline.
  check [ $bytes -gt 100000 ]
  echo second pass: a short deadline and an image optimization
  echo make an unoptimized result very likely on the second pass.
  echo $cmd; bytes=$($cmd | wc -c)
  check [ $bytes -gt 100000 ]
  echo Finally make sure the image gets optimized eventually.
  # We don't know how long it will take; if you do the fetch with
  # no delay it will probably fail because bash is faster than image
  # optimization, so use fetch_until.
  fetch_until $TEST_ROOT/ipro/wait/short/Puzzle.jpg 'wc -c' 100000 "" -lt
fi

start_test AddResourceHeaders works for pagespeed resources.
URL="$TEST_ROOT/compressed/hello_js.custom_ext.pagespeed.ce.HdziXmtLIV.txt"
fetch_until -save "$URL" 'fgrep -c text/javascript' 1 --save-headers
HTML_HEADERS=$(extract_headers $FETCH_FILE)
check_from "$HTML_HEADERS" grep -q "^X-Foo: Bar"

start_test long url handling
# This is an extremely long url, enough that it should give a 4xx server error.
OUT=$($CURL -sS -D- "$TEST_ROOT/$(head -c 10000 < /dev/zero | tr '\0' 'a')")
check_from "$OUT" grep -q "414 Request-URI Too Large\|414 Request-URI Too Long"

function get_controller_pid() {
  grep "Controller running with PID " $ERROR_LOG | tail -n 1 | awk '{print $NF}'
}

if [ "$RUN_CONTROLLER_TEST" = "on" ]; then
  start_test babysitter process restarts controller when killed

  controller_pid=$(get_controller_pid)
  check [ ! -z "$controller_pid" ]  # Controller PID should be in log.

  # We should see the babysitter process starting too.
  check grep "Babysitter running with PID " $ERROR_LOG

  function count_watcher_messages() {
    grep -c "Watching the root process to exit if it dies." $ERROR_LOG
  }

  # And the ProcessDeathWatcherThread should be running.
  echo "Checking that we're watching the right processes."
  initial_watcher_count=$(count_watcher_messages)
  # On nginx this will be 1; on apache it will be 2 because apache starts twice to
  # check its config.
  check [ $initial_watcher_count -gt 0 ]

  # Now kill the controller and verify that it gets restarted.
  kill "$controller_pid"

  function did_controller_restart() {
    new_controller_pid=$(get_controller_pid)

    # If there's a new PID, that means it was restarted.
    test ! -z "$new_controller_pid" -a "$new_controller_pid" != "$controller_pid"
  }

  echo -n "Waiting for babysitter to restart controller ..."
  SECONDS=0
  while ! did_controller_restart && [ $SECONDS -lt 10 ]; do
    echo -n .
    sleep 0.1
  done
  echo

  check did_controller_restart

  echo "Checking that babysitter reported controller death..."
  grep "Controller process $controller_pid exited with wait status" \
    $ERROR_LOG > /dev/null

  # The ProcessDeathWatcherThread should have been restarted (it's hosted by the
  # controller thread, not the babysitter). This message may be delayed slightly
  # under valgrind, so allow a few retries.
  echo "Checking again that we're watching the right processes."
  final_watcher_count=$(count_watcher_messages)
  SECONDS=0
  while [ $final_watcher_count -eq $initial_watcher_count -a\
          $SECONDS -lt 2 ]; do
    sleep 0.1
    final_watcher_count=$(count_watcher_messages)
  done
  check [ $final_watcher_count -eq $((initial_watcher_count + 1)) ]
elif [ "$FIRST_RUN" = "true" ]; then
  start_test With controller off, there should be no pid.
  # This should only be checked in the first frun because there may
  # be a leftover pid from an earlier run in the error.log.  Related:
  # we must ensure that whenever FIRST_RUN is true, the logs must
  # be cleared before running the test script.
  check [ "$(get_controller_pid)" = "" ];
fi

start_test Strip subresources default behaviour
URL="$TEST_ROOT/strip_subresource_hints/default/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip multiple subresources default behaviour
URL="$TEST_ROOT/strip_subresource_hints/default/multiple_subresource_hints.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources default behaviour disallow
URL="$TEST_ROOT/strip_subresource_hints/default/disallowtest.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources preserve on
URL="$TEST_ROOT/strip_subresource_hints/preserve_on/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources preserve off
URL="$TEST_ROOT/strip_subresource_hints/preserve_off/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_not_from "$OUT" grep -q -F "rel=\"subresource"

start_test Strip subresources rewrite level passthrough
URL="$TEST_ROOT/strip_subresource_hints/default_passthrough/index.html"
echo $WGET_DUMP $URL
OUT=$($WGET_DUMP $URL)
check_from "$OUT" grep -q -F "rel=\"subresource"

