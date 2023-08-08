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
    <div><img src="/esp32_photo_url"                   id="dynamicImage1" ></div>
    <div><img src="/mlx90640_esp32_addition_photo_url" id="dynamicImage2" ></div>
    <div><img src="/mlx90640_photo_url"                id="dynamicImage3" ></div>
  </div>

</body>
<script>
  
  // 更新網頁資訊
  setInterval(function() {
    // 獲取照片地址
    var img1 = document.getElementById("dynamicImage1");   // 獲取圖片元素:
    // Update the image source with a new timestamp to avoid caching
    img1.src = "/esp32_photo_url?" + new Date().getTime();

    var img2 = document.getElementById("dynamicImage2");   // 獲取圖片元素:
    // Update the image source with a new timestamp to avoid caching
    img2.src = "/mlx90640_esp32_addition_photo_url?" + new Date().getTime();

    var img3 = document.getElementById("dynamicImage3");   // 獲取圖片元素:
    // Update the image source with a new timestamp to avoid caching
    img3.src = "/mlx90640_photo_url?" + new Date().getTime();
  }, 5000);

  setInterval(function(){
    // 顯示環境溫度
    var textElement = document.getElementById('dynamicText_min_temp');
    var xhr = new XMLHttpRequest();
    xhr.open('GET', '/getText_mintemp', true);
    xhr.onload = function() {
        if (xhr.status === 200) {
            textElement.innerText = xhr.responseText;
        }
    };
  }, 3000);

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
