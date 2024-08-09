const char htmlFooter[] PROGMEM = (R"====(
 &nbsp;
</div>
</div>
  <script type="text/javascript">
    onload = () => {
      const c = setTimeout(() => {
        document.body.classList.remove("not-loaded");
        clearTimeout(c);
      }, 1000);
    };
  </script>
  </body>
</html>
)====");
