function count_blue_css_cache_extend() {
  fgrep -c "blue.css.pagespeed.ce"
}

start_test single redirect works
# Test single redirect
URL=http://redirecting-fetch.example.com/redir_to_test/
URL+=styles/A.blue.css.pagespeed.cf.0.css
echo "$URL"
echo "http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL 2>&1)
check_from "$OUT" fgrep -qi 'content-type: text/css'
check_from "$OUT" fgrep -qi '.yellow{'
check_not_from "$OUT" fgrep -qi 'location:'

URL=http://redirecting-fetch.example.com/redir_to_test/
URL+=inline_css.html?PageSpeedFilters=extend_cache
http_proxy=$SECONDARY_HOSTNAME fetch_until "$URL" count_blue_css_cache_extend 1

start_test multi redirect works
URL=http://redirecting-fetch.example.com/redir_to_test/
URL+=styles/A.1.css.pagespeed.cf.0.css
echo "$URL"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL 2>&1)
check_from "$OUT" fgrep -qi 'content-type: text/css'
check_from "$OUT" fgrep -qi '.yellow{'
check_not_from "$OUT" fgrep -qi 'location:'

start_test disallowed redirects are not followed
URL=http://redirecting-fetch.example.com/redir_to_test/
URL+=styles/A.redirtodisallowed.css.pagespeed.cf.0.css
echo "$URL"
OUT=$(http_proxy=$SECONDARY_HOSTNAME \
    $CURL -o/dev/null -sS --write-out '%{http_code}\n' "$URL")
check [ "$OUT" = "404" ]

start_test max redirects is respected
URL=http://redirecting-fetch-single-only.example.com/redir_to_test/
URL+=styles/A.1.css.pagespeed.cf.0.css
echo "$URL"
OUT=$(http_proxy=$SECONDARY_HOSTNAME \
    $CURL -o/dev/null -sS --write-out '%{http_code}\n' "$URL")
check [ "$OUT" = "404" ]


start_test temp redirects followed when configured to
URL=http://redirecting-fetch-temp.example.com/redir_to_test/
URL+=styles/A.1.css.pagespeed.cf.0.css
echo "$URL"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL 2>&1)
check_from "$OUT" fgrep -qi 'content-type: text/css'
check_from "$OUT" fgrep -qi '.yellow{'
check_not_from "$OUT" fgrep -qi 'location:'

start_test simple csp honored
# A resource that will run via a single redirect should work
URL=http://redirecting-fetch-csp.example.com/redir_to_test/
URL+=styles/A.blue.css.pagespeed.cf.0.css
echo "$URL"
echo "http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL 2>&1)
check_from "$OUT" fgrep -qi 'content-type: text/css'
check_from "$OUT" fgrep -qi '.yellow{'
check_not_from "$OUT" fgrep -qi 'location:'


function csp_query() {
    URL="http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT/redir_to_test/csp.php?PageSpeedFilters=debug,$3"
    tmp_csp=$(echo -e "$1" | od -An -tx1 | tr ' ' % | xargs printf "%s")
    tmp_html=$(echo -e "$2" | od -An -tx1 | tr ' ' % | xargs printf "%s")
    echo "csp=$tmp_csp&html=$tmp_html" > /tmp/pagespeed.post.tmp
    http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP --post-file=/tmp/pagespeed.post.tmp $URL 2>&1
}
function csp_until() {
    URL="http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT/redir_to_test/csp.php?PageSpeedFilters=debug,$3"
    tmp_csp=$(echo -e "$1" | od -An -tx1 | tr ' ' % | xargs printf "%s")
    tmp_html=$(echo -e "$2" | od -An -tx1 | tr ' ' % | xargs printf "%s")
    echo "csp=$tmp_csp&html=$tmp_html" > /tmp/pagespeed.post.tmp
    http_proxy=$SECONDARY_HOSTNAME fetch_until "$URL" "$4" $5 --post-file=/tmp/pagespeed.post.tmp
}

html='
<link rel="stylesheet" href="styles/all_styles.css">
<link rel="stylesheet" href="styles/blue.css">
'

start_test "No CSP, inline CSS authorized"
csp_until "" "$html" "inline_css" 'grep -c yellow.css' 0

start_test "default https CSP. inline CSS unauthorized"
OUT=$(csp_query "default-src https:;" "$html" "inline_css")
check_from "$OUT" egrep '.link rel..stylesheet. href..styles.all_styles.css......The preceding resource was not rewritten because CSP disallows its fetch'
check_from "$OUT" egrep '.link rel..stylesheet. href..styles.blue.css......The preceding resource was not rewritten because CSP disallows its fetch'

start_test "inline CSS unauthorized because of redirect not in policy"
OUT=$(csp_query "default-src https:; style-src http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT" "$html" "inline_css")
check_from "$OUT" egrep '<link rel="stylesheet" href="styles/blue.css"><!--PageSpeed output .by ci. not permitted by Content Security Policy-->'

html='
<link rel="stylesheet" href="styles/blue.css">
'

start_test "inline CSS authorized, redirect authorized in policy"
csp_until "default-src style-src http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT http://redirecting-fetch.example.com:$APACHE_SECONDARY_PORT" "$html" "inline_css" 'grep -c .yellow..background.color..yellow..' 1

start_test "Cache extend CSS authorized, redirect authorized in policy"
csp_until "style-src http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT http://redirecting-fetch.example.com:$APACHE_SECONDARY_PORT" "$html" "extend_cache" 'grep -c styles/blue.css.pagespeed.ce.' 1

start_test "Cache extend CSS not authorized"
OUT=$(csp_query "default-src https:" "$html" "extend_cache")
check_from "$OUT" egrep '<link rel="stylesheet" href="styles/blue.css"><!--The preceding resource was not rewritten because CSP disallows its fetch-->'

start_test "Cache extend CSS redirect not authorized in policy"
OUT=$(csp_query "style-src http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT" "$html" "extend_cache")
check_from "$OUT" egrep '<!--PageSpeed output .by CacheExtender. not permitted by Content Security Policy-->'


html='
<link rel="stylesheet" href="styles/yellow.css">
<link rel="stylesheet" href="styles/blue.css">
'

start_test "Combine css authorized"
csp_until "style-src http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT http://redirecting-fetch.example.com:$APACHE_SECONDARY_PORT" "$html" "combine_css" 'grep -c styles.yellow.css.blue.css.pagespeed.cc.a071ckd1d5.css' 1

start_test "Combine css not authorized"
OUT=$(csp_query "default-src https:" "$html" "combine_css")
check_from "$OUT" egrep '<link rel="stylesheet" href="styles/yellow.css"><!--The preceding resource was not rewritten because CSP disallows its fetch-->'
check_from "$OUT" egrep '<link rel="stylesheet" href="styles/blue.css"><!--The preceding resource was not rewritten because CSP disallows its fetch-->'

html='
<link rel="stylesheet" href="styles/yellow.css?a=1">
<link rel="stylesheet" href="styles/yellow.css?a=2">
<link rel="stylesheet" href="styles/blue.css?a=1">
<link rel="stylesheet" href="styles/blue.css?a=2">
<link rel="stylesheet" href="styles/yellow.css?a=3">
<link rel="stylesheet" href="styles/yellow.css?a=4">
'

start_test "Combine css redirect not authorized partitions OK"
csp_until "style-src http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT" "$html" "combine_css" 'grep -c .pagespeed.cc' 2
OUT=$(csp_query "style-src http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT" "$html" "combine_css")
check_from "$OUT" egrep 'yellow.css,qa..1.yellow.css.qa..2.pagespeed.cc.a071ckd1d5.css'
check_from "$OUT" egrep 'styles/blue.css.a.1'
check_from "$OUT" egrep 'styles/blue.css.a.2'
check_from "$OUT" egrep 'yellow.css,qa..3.yellow.css.qa..4.pagespeed.cc.a071ckd1d5.css'

start_test "Multiple filters redirect not authorized partitions OK"
OUT=$(csp_query "style-src http://redirecting-fetch-csp.example.com:$APACHE_SECONDARY_PORT" "$html" "combine_css,inline_css,extend_cache")
check_from "$OUT" egrep 'yellow.css,qa..1.yellow.css.qa..2.pagespeed.cc.a071ckd1d5.css'
check_from "$OUT" egrep '<link rel="stylesheet" href="styles/blue.css.a=1">'
check_from "$OUT" egrep '<link rel="stylesheet" href="styles/blue.css.a=2">'
check_from "$OUT" egrep 'yellow.css,qa..3.yellow.css.qa..4.pagespeed.cc.a071ckd1d5.css'

