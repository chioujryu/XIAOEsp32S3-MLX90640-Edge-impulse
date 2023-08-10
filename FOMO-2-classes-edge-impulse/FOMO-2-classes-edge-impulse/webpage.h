// index_html 是一個字串常數,被定義為PROGMEM(Program Memory)。
// 這意味著該HTML字串被編譯到ESP32的PROGRAM FLASH中,而非 RAM 中。

const char index_html[] PROGMEM = R"rawliteral( 
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
        body {
          background-color: #e6e6e6; 
          text-align:center; 
          font-family: Arial, Helvetica, Sans-Serif; 
          Color: black; 
        }
        #image_container {
          display: flex; /* 新增這一行 */
          justify-content: center; /* 使圖片居中 */
        }
        #image_container div {
          width: 100%; /* 這裡設置每個圖片容器的寬度 */
        }
  </style>
</head>
<body>
  <div id="container">
    <h1>Object detection and Thermal detection</h2>

    <h3>Enviroment temp</h3>
    <p id="dynamicText_min_temp"></p>
    <p id="dynamicText_max_temp"></p>
    <p id="dynamicText_ave_temp"></p>
    
  </div>
  <div id="image_container">
    <div><img src="/mlx90640_esp32_addition_photo_url" id="dynamicImage2" width="30%"></div>  
  </div>

</body>
<script>
  
  // 更新網頁資訊
  setInterval(function() {
    var img2 = document.getElementById("dynamicImage2");   // 獲取圖片元素:
    // Update the image source with a new timestamp to avoid caching
    img2.src = "/mlx90640_esp32_addition_photo_url?" + new Date().getTime();
  }, 5000);

}
</script>
</html>)rawliteral";
