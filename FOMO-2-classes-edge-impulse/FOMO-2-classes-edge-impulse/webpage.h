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
      <button onclick="savePhoto(); updateText()">SAVE PHOTO</button>
    </p>
    
    <p>[New Captured Image takes 5 seconds to appear]</p>
    <p id="dynamicText"> </p>
    
  </div>
  <div><img src="/saved_thermal_esp32_addition_photo" id="photo3" width="30%"></div>
        <hr>
      <h5> @2023 Marcelo Rovai - MJRoBot.org </h5>
</body>
<script>
  var deg = 0;
  // setInterval
  setInterval(reload_image, 3000);
  function reload_image() {
    /*這段程式碼使用 XMLHttpRequest 對象發送 AJAX 請求,實現不刷新頁面就可以更新內容的效果,每行代碼的作用如下:*/
    var xhr = new XMLHttpRequest();     // 創建一個 XMLHttpRequest 對象,命名為 xhr。XMLHttpRequest 是瀏覽器提供的JavaScript API,用於發送 HTTP 請求與接收 HTTP 響應。
    xhr.open('GET', "/get_image", true);  // 調用 xhr 對象的 open() 方法,配置這次請求的方法、URL 和是否異步。 1. 方法傳 'GET',表示發送 GET 請求;   2. URL 傳 '/capture',表示請求的接口地址; 3. 第三個參數 true 表示這是一個異步請求。
    xhr.send();                         // 調用 xhr 的 send() 方法,真正發送這次 AJAX 請求。
    wait(2000);                          // 等待 500 毫秒,給服務器一點響應的時間。
    location.reload();                  // 調用 location.reload() 方法,重新加載當前頁面,以獲取更新後的內容。
  }

  function savePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/save", true);
    xhr.send();
    wait(500);
    location.reload();
  }

  function updateText() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/getText", true);

    xhr.onreadystatechange = function() {
      if (xhr.readyState == 4 && xhr.status == 200) {
        var textElement = document.getElementById("dynamicText");
        var updatedText = xhr.responseText;
        textElement.innerHTML = updatedText;
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
