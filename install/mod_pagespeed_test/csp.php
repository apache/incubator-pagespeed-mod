<?php
# default-src https:; style-src http://redirecting-fetch-csp.example.com:8083
# TODO(oschaaf): probably don't want to host this file
# TODO(oschaaf): move out to mod_pagespeed_test

header("Content-Security-Policy: " . $_POST['csp']);
?>
<html>
  <head>
    <title>csp example</title>
<?php
    echo $_POST['html'];
?>
  </head>
  <body>
    <div class="blue yellow big bold">
      CSP example
    </div>
  </body>
</html>
