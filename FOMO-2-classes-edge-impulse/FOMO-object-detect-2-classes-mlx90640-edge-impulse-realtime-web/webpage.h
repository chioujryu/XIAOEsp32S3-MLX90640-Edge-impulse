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
    <p>Max Temperature: <span id="maxTemp"></span></p>
    <p>Min Temperature: <span id="minTemp"></span></p>
    <p>Avg Temperature: <span id="avgTemp"></span></p>
    <p><button onclick="switchPhoto()">SWITCH PHOTO</button></p>
    
  </div>
  <div id="image_container">
    <div><canvas id="myCanvas1" width="320px" height="240px"></canvas></div>
  </div> 

  <div>
    <p id="dynamicText"> </p>   
  </div>

</body>
<script>
  var w = {"1":50, "2":25};
  var h = {"1":50, "2":50};
  var color = {"1":"rgb(255,0,0)", "2":"rgb(0,0,255)"}
  function fetchData() {
    fetch('/data').then(response => response.json()).then(data => {
      // 顯示圖片
      var ctx1 = document.getElementById("myCanvas1").getContext("2d");
      ctx1.font = "20px sans-serif"
      var imgObject1 = new Image();
      var dynamicText = "";
      imgObject1.src =  'data:image/jpeg;base64,' + data.image;
      imgObject1.addEventListener('load',function(){
      ctx1.drawImage(imgObject1,0,0);
      for (var i=0; i<data.bbox_count; i++){
        ctx1.strokeStyle = color[data.label[i]];
        ctx1.fillStyle = color[data.label[i]];
        ctx1.strokeRect(data.centerX[i]-(w[data.label[i]]/2), data.centerY[i]-(h[data.label[i]]/2), w[data.label[i]], h[data.label[i]]);
        ctx1.fillText(i, data.centerX[i]-(w[data.label[i]]/2), data.centerY[i]-(h[data.label[i]]/2));
        dynamicText += "ID:" + i + ", label:" + data.label[i] + ", conf:" + data.conf[i] + ", maxTemp:" + data.maxTemp[i] + ", minTemp:" + data.minTemp[i] + ", avgTemp:" + data.avgTemp[i] + "\n";}
      document.getElementById('dynamicText').innerText = dynamicText;
      })

      // 顯示溫度數據
      document.getElementById('maxTemp').innerText = data.maxTemp_t;
      document.getElementById('minTemp').innerText = data.minTemp_t;
      document.getElementById('avgTemp').innerText = data.avgTemp_t;
    });
  }
  // 每 1 秒請求一次數據
  setInterval(fetchData, 1000);

  function switchPhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/switch", true);
    xhr.send();
  }

  function wait(ms){
   var start = new Date().getTime();
   var end = start;
   while(end < start + ms) {
     end = new Date().getTime();
  }
}
</script>
</html>)rawliteral";
