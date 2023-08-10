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
  </style>
</head>
<body>
  <div id="container">
    <h2>XIAO ESP32S3-Sense Image Capture</h2>
    <p>Max Temperature: <span id="maxTemp"></span></p>
    <p>Min Temperature: <span id="minTemp"></span></p>
    <p>Avg Temperature: <span id="avgTemp"></span></p>
    
  </div>
  <div><img id="image" /></div>
  
        <hr>
      <h5> @2023 Marcelo Rovai - MJRoBot.org </h5>
</body>
<script>

  function fetchData() {
    fetch('/data').then(response => response.json()).then(data => {
      // 顯示圖片
      document.getElementById('image').src = 'data:image/jpeg;base64,' + data.image;

      // 顯示溫度數據
      document.getElementById('maxTemp').innerText = data.maxTemp;
      document.getElementById('minTemp').innerText = data.minTemp;
      document.getElementById('avgTemp').innerText = data.avgTemp;
    });
  }
  // 每 3 秒請求一次數據
  setInterval(fetchData, 1000);
  
</script>
</html>)rawliteral";
