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
check_from "$OUT" fgrep -qi '200 OK'
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
check_from "$OUT" fgrep -qi '200 OK'
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
check_from "$OUT" fgrep -qi '200 OK'
check_from "$OUT" fgrep -qi 'content-type: text/css'
check_from "$OUT" fgrep -qi '.yellow{'
check_not_from "$OUT" fgrep -qi 'location:'

start_test simple csp honored
# Test single redirect
URL=http://redirecting-fetch-csp.example.com/redir_to_test/
URL+=styles/A.blue.css.pagespeed.cf.0.css
echo "$URL"
echo "http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL 2>&1)
check_from "$OUT" fgrep -qi '200 OK'
check_from "$OUT" fgrep -qi 'content-type: text/css'
check_from "$OUT" fgrep -qi '.yellow{'
check_not_from "$OUT" fgrep -qi 'location:'

URL=http://redirecting-fetch-csp.example.com/redir_to_test/
URL+=csp.php?PageSpeedFilters=extend_cache,debug
echo "$URL"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL 2>&1)
check_from "$OUT" fgrep -qi '200 OK'
check_from "$OUT" fgrep -qi 'blue.css'
check_from "$OUT" fgrep -qi 'The preceding resource was not rewritten because CSP disallows its fetch'

# TODO(oschaaf): ^^ test expectation should be more specific.
# TODO(oschaaf): test below fails, html seems to work differently.

URL=http://redirecting-fetch-csp.example.com/redir_to_test/
URL+=inline_css.html?PageSpeedFilters=extend_cache,debug
echo "$URL"
OUT=$(http_proxy=$SECONDARY_HOSTNAME $WGET_DUMP $URL 2>&1)
check_from "$OUT" fgrep -qi '200 OK'
check_from "$OUT" fgrep -qi 'blue.css'
check_from "$OUT" fgrep -qi 'The preceding resource was not rewritten because CSP disallows its fetch'
exit 1
