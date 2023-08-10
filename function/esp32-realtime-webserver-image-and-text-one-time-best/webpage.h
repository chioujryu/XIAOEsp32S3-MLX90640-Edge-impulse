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

    <p>
      <button onclick="rotatePhoto();">ROTATE</button>
      <button onclick="savePhoto()">SAVE PHOTO</button>
    </p>
    
    <p>[New Captured Image takes 5 seconds to appear]</p>
    <p id="dynamicText"></p>
    <p id="mintemptext"></p>
    <p id="maxtemptext"></p>
    <p id="avetemptext"></p>
    
  </div>
  <div><img src="/get_esp32_photo_url" id="dynamicImage1" width="30%"></div>
  
        <hr>
      <h5> @2023 Marcelo Rovai - MJRoBot.org </h5>
</body>
<script>
  
  // 每隔1秒，拍照一次
  setInterval(function() {
    // 拍照
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();

    // 獲取照片地址
    var img = document.getElementById("dynamicImage1");   // 獲取圖片元素:
    // Update the image source with a new timestamp to avoid caching
    img.src = "/get_esp32_photo_url?" + new Date().getTime();

    
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/get_temp', true);
    xhr.onload = function() {
      
      // 解析response
      var data = JSON.parse(xhr.response);
  
      var minTemp = data.minTemp;
      var maxTemp = data.maxTemp; 
      var avgTemp = data.avgTemp;
  
      document.getElementById('mintemptext').innerText = minTemp;
      document.getElementById('maxtemptext').innerText = maxTemp;
      document.getElementById('avetemptext').innerText = avgTemp;
    }  
    xhr.send();
  }, 1000);


  function getImage() {
  
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/photo.jpg'); 
    xhr.onload = function() {
      if(xhr.status == 200){
        document.getElementById('camera-img').src = "/photo.jpg";
      } else {
        console.log('getImage failed!'); 
      }
    }
    xhr.send();
  }


  function savePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/save", true);
    xhr.send();
    wait(500);
    location.reload();
  }
  
  function rotatePhoto() {
    var deg = 0;
    var img = document.getElementById("dynamicImage1");   // 獲取圖片元素:
    deg += 180;     // 累加旋轉角度:

    /*通過判斷 deg 除以 90 的餘數是否為奇數,來決定圖片的旋轉方向。
      如果為奇數,表示垂直方向旋轉,給容器設置 vert 類;
      如果為偶數,表示水平方向旋轉,給容器設置 hori 類。*/
    if(isOdd(deg/90)){ document.getElementById("container").className = "vert"; }   // 判斷旋轉方向:
    else{ document.getElementById("container").className = "hori"; }
    img.style.transform = "rotate(" + deg + "deg)";   // 設置圖片的 transform 屬性: 根據計算好的 deg 角度,通過 CSS transform 屬性轉換圖片。
  }

  function updateText() {
    var dataElement = document.getElementById('dynamicText');
    var xhr = new XMLHttpRequest();
    
    xhr.open('GET', "/getText", true);
    // Set the callback for when the request completes
    xhr.onload = function() {
        if (xhr.status === 200) {
            // Update the data element with the new data
            dataElement.innerText = xhr.responseText;
        }
    };

    xhr.send();
  }
  

  /*  這個函數是用來判斷一個數字是否為奇數的:
   n % 2 可以獲取參數n除以2的餘數
   Math.abs() 用來獲取絕對值,因為餘數可能是負數
   判斷餘數是否等於1,如果等於1,則為奇數
   最後return回結果,true為奇數,false為偶數
   */
  function isOdd(n) { return Math.abs(n % 2) == 1; }

  function wait(ms){
     var start = new Date().getTime();
     var end = start;
     while(end < start + ms) {
       end = new Date().getTime();
     }
  }
</script>
</html>)rawliteral";
