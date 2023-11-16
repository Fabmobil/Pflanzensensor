String htmlHeader = F(R"====(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8" http-equiv="refresh" content="10">
  <title>Fabmobil Pflanzensensor</title>
  <style>
    h1, h2, h3, h4, h5 {
      padding: 0.5em;
      margin-bottom: 0.5em;
      font-weight: bold;
      font-style: normal;
      text-transform: uppercase;
      color: white;
      background-color: #560245;
      text-align: left;
      border-radius: 0px 0px 0px 30px;
    }

    input {
      font-size: 2em;
    }

    h1 {
      font-size: 2.5em;
      margin-left: 1em;
    }

    h2 {
      font-size: 2.25em;
      margin-left: 2em;
    }

    h3 {
      font-size: 2.0em;
      margin-left: 3em;
    }

    h4 {
      font-size: 1.75em;
      margin-left: 4em;
    }

    h5 {
      font-size: 1.5em;
      margin-left: 5em;
    }

    strong {
      font-weight: bolder;
      color: #560245;
      font-size: 1.5em;
    }

    p, ul, ol {
      line-height: 1.4;
      color: #4a4a4a;
      font-weight: normal;
      text-align: left;
      margin-left: 5.5em;
      margin-bottom: 0;
      margin-top: 0;
      padding-left: 1em;
      padding-right: 2em;
      padding-top: 0;
      padding-bottom: 0.5em;
      font-size: 2em;
    }

    a {
      color: #560245;
      text-decoration: none;
    }

    a:hover {
      text-decoration: underline overline;
    }

    figure {
      padding-left: 2em;
      margin-left: 5.5em;
      padding-right: 2em;
    }

    img {
      max-width: 100%;
      height: auto;
    }

    figcaption {
      display: none;
    }

    #container {
      border-right: 2em solid #560245;
      border-radius: 30px;
    }
  </style>
</head>
<body>
<div id="container">
<h1>Fabmobil Pflanzensensor</h1>
)====");
