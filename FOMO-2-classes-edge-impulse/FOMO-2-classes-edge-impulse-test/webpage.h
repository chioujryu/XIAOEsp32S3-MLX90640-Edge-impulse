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
    <p id="mintemptext"></p>
    <p id="maxtemptext"></p>
    <p id="avetemptext"></p>
    
  </div>
  <div id="image_container">
    <div><img src="/mlx90640_esp32_addition_photo_url" id="dynamicImage2" width="30%"></div>  
  </div>

</body>
<script>
  
  // 更新網頁資訊
  setInterval(function() {
    // 拍照
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture_and_inference", true);
    xhr.send();

    var img2 = document.getElementById("dynamicImage2");   // 獲取圖片元素:
    // Update the image source with a new timestamp to avoid caching
    img2.src = "/mlx90640_esp32_addition_photo_url?" + new Date().getTime();

    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/get_text', true);
    xhr.onload = function() {
      
      // 解析response
      var data = JSON.parse(xhr.response);
  
      var minTemp = "Min Temp: " + data.minTemp;
      var maxTemp = "Max Temp: " + data.maxTemp; 
      var avgTemp = "Ave Temp: " + data.avgTemp;
  
      document.getElementById('mintemptext').innerText = minTemp;
      document.getElementById('maxtemptext').innerText = maxTemp;
      document.getElementById('avetemptext').innerText = avgTemp;
    }  
    xhr.send();
    
  }, 6000);

</script>
</html>)rawliteral";
