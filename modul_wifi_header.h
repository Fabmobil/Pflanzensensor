String htmlHeader = F(R"====(
<!DOCTYPE html>
<html lang="de">
<head>
  <meta charset="utf-8">
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
      animation-name: fadeInUp;
      animation-duration: 1s;
      animation-fill-mode: both;
    }

    h1 {
      font-size: 2.5em;
    }

    h2 {
      font-size: 2.25em;
      animation-delay: 0.2s;
    }

    h3 {
      font-size: 2.0em;
      animation-delay: 0.4s;
    }

    h4 {
      font-size: 1.75em;
      animation-delay: 0.6s;
    }

    h5 {
      font-size: 1.5em;
      animation-delay: 0.8s;
    }

    strong {
      font-weight: bolder;
      color: #560245;
      font-size: 1.5em;
    }

    p, ul, ol, input {
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
      animation-name: fadeInLeft;
      animation-duration: 1s;
      animation-fill-mode: both;
    }

    a {
      color: #560245;
      text-decoration: none;
      animation-name: link;
      animation-duration: 8s;
      animation-iteration-count: infinite;
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
      border-radius: 30px;
      box-shadow: 10px 10px;
      animation-name: zoomIn;
      animation-duration: 1s;
      animation-fill-mode: both;
    }

    figcaption {
      display: none;
    }

    #container {
      border-right: 2em solid #560245;
      border-radius: 30px;
      animation-name: fadeInRight;
      animation-duration: 1s;
      animation-fill-mode: both;
    }

    @keyframes fadeInUp {
      from {
        opacity: 0;
        transform: translateY(20px);
      }
      to {
        opacity: 1;
        transform: translateY(0);
      }
    }

    @keyframes fadeInLeft {
      from {
        opacity: 0;
        transform: translateX(-20px);
      }
      to {
        opacity: 1;
        transform: translateX(0);
      }
    }

    @keyframes fadeInRight {
      from {
        opacity: 0;
        transform: translateX(20px);
      }
      to {
        opacity: 1;
        transform: translateX(0);
      }
    }

    @keyframes zoomIn {
      from {
        transform: scale(0);
      }
      to {
        transform: scale(1);
      }
    }
  </style>
</head>
<body>
<div id="container">
<h1>Fabmobil Pflanzensensor</h1>
)====");
