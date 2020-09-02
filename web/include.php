<?php
// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

ini_set('arg_separator.output', '&amp;');
ini_set('url_rewriter.tags', 'a=href,form=');

define('IS_ADMIN',
       isset($_REQUEST['key']) &&
       sha1($_REQUEST['key']) == 'a299db9fb7cea3851a60d8728a486633ac9f0d09');

function html_header($title) {
  if (IS_ADMIN)
    output_add_rewrite_var('key', $_REQUEST['key']);

  echo <<< HTML
<!DOCTYPE html>
<html>
<head>
<title>Alive2: $title</title>
<script src="https://cdn.jsdelivr.net/npm/chart.js@2/dist/Chart.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/jquery@3/dist/jquery.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/tablesorter@2/dist/js/jquery.tablesorter.combined.min.js"></script>
<link rel="stylesheet" href="https://cdn.jsdelivr.net/npm/tablesorter@2/dist/css/theme.blue.min.css">
<style>
table {
  border-collapse: collapse
}

table, th, td {
  padding: 3px;
  border: 1px solid black
}
</style>
</head>
<body>
<h1>$title</h1>

HTML;

  if (IS_ADMIN)
    echo "<p style=\"text-align:center; color:red\"><b>ADMIN MODE</b></p>\n";
}

function html_footer() {
  $vars = '';
  if (!empty($_REQUEST['test']))
    $vars = '?hash='.@$_REQUEST['hash'];

  echo <<< HTML
<p>&nbsp;</p>
<p><a href="index.php$vars">&lt;-- Back</a></p>
</body>
</html>

HTML;
}
