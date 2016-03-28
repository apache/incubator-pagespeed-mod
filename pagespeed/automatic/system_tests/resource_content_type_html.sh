function verify_nosniff {
  leaf="$1"
  content_type="$2"
  URL=$REWRITTEN_ROOT/$leaf
  OUT=$($CURL -D- -o/dev/null -sS "$URL")
  check_from "$OUT" grep '^HTTP.* 200 OK'
  check_from "$OUT" grep '^Content-Type: '"$content_type"''
  check_from "$OUT" grep '^X-Content-Type-Options: nosniff'
}

function verify_404 {
  leaf="$1"
  URL=$REWRITTEN_ROOT/$leaf
  OUT=$($CURL -D- -o/dev/null -sS "$URL")
  check_from "$OUT" grep '^HTTP.* 404 Not Found'
}

# test that all the filters do fine with one of our content types
start_test js minification css
verify_nosniff styles/big.css.pagespeed.jm.0.foo text/css

start_test image spriting css
verify_nosniff styles/big.css.pagespeed.is.0.foo text/css

start_test image compression css
verify_nosniff styles/xbig.css.pagespeed.ic.0.foo text/css

start_test cache extension css
verify_nosniff styles/big.css.pagespeed.ce.0.foo text/css


# test that we also do fine with the other content types we generate
start_test js minification js
verify_nosniff rewrite_javascript.js.pagespeed.jm.0.foo \
  "application/\(x-\)\?javascript"

start_test js minification png
verify_nosniff images/Cuppa.png.pagespeed.jm.0.foo image/png

start_test js minification gif
verify_nosniff images/IronChef2.gif.pagespeed.jm.0.foo image/gif

start_test js minification jpg
verify_nosniff images/Puzzle.jpg.pagespeed.jm.0.foo image/jpeg

start_test js minification pdf
verify_nosniff example.pdf.pagespeed.jm.0.foo application/pdf


# test that we 404 html
start_test js minification html
verify_404 index.html.pagespeed.jm.0.foo

start_test image spriting html
verify_404 index.html.pagespeed.is.0.foo

start_test image compression html
verify_404 xindex.html.pagespeed.ic.0.foo

start_test cache extension html
verify_404 index.html.pagespeed.ce.0.foo


# test that we 404 svgs too
start_test js minification svg
verify_404 images/schedule_event.svg.pagespeed.jm.0.foo

