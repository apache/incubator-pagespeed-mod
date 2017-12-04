<?php
header("Content-Security-Policy: default-src https:; style-src http://redirecting-fetch-csp.example.com:8083");
?>
<html>
  <head>
    <title>csp example</title>
    <link rel="stylesheet" href="styles/all_styles.css">
    <link rel="stylesheet" href="styles/blue.css">
    <link rel="stylesheet" href="styles/bold.css" media="not decodable">
    <link rel="stylesheet" href="styles/yellow.css" media=", ,print, screen ">
    <link rel="stylesheet" href="styles/rewrite_css_images.css" media="all">
  </head>
  <body>
    <div class="blue yellow big bold">
      CSP example
    </div>
  </body>
</html>
