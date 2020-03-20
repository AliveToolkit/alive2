<?php
// Copyright (c) 2018-present The Alive2 Authors.
// Distributed under the MIT license that can be found in the LICENSE file.

function html_header($title) {
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
  border: 1px solid black
}
</style>
</head>
<body>
<h1>$title</h1>

HTML;
}

function html_footer() {
  echo <<< HTML
</body>
</html>

HTML;
}
