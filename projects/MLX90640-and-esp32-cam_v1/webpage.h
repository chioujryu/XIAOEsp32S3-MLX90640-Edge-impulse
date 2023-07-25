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
      <button onclick="capturePhoto()">CAPTURE</button>
      <button onclick="savePhoto()">SAVE PHOTO</button>
    </p>
    
    <p>[New Captured Image takes 5 seconds to appear]</p>
    <p>First press CAPTURE, then press SAVE PHOTO</p>
    
  </div>
  <div><img src="/saved_photo" id="photo" width="50%"></div>
        <hr>
      <h5> @2023 Marcelo Rovai - MJRoBot.org </h5>
</body>
<script>
  var deg = 0;
  function capturePhoto() {
    var xhr = new XMLHttpRequest();  // 創立一個 XMLHttpRequest 的物件，名為 xhr
    xhr.open('GET', "/capture", true);  // 當獲取到 "/capture" 的時候
    xhr.send();
    wait(500);
    location.reload();
  }

  function savePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/save", true);
    xhr.send();
    wait(500);
    location.reload();
  }
  
  function rotatePhoto() {
    var img = document.getElementById("photo");
    deg += 180;
    if(isOdd(deg/90)){ document.getElementById("container").className = "vert"; }
    else{ document.getElementById("container").className = "hori"; }
    img.style.transform = "rotate(" + deg + "deg)";
  }
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
