var http = require("http"),
    url = require("url"),
    path = require("path"),
    fs = require("fs"),
    url = require("url"),
    port = process.argv[2] || 8888;

var fileUser1 = "/home/nqdinh/esp-fw-ota/user1.bin";
var fileUser2 = "/home/nqdinh/esp-fw-ota/user2.bin";
// var fileUser1 = "https://dl.dropboxusercontent.com/s/jwnjhet4tngh3nh/user1.bin?dl=0";
// var fileUser2 = "https://dl.dropboxusercontent.com/s/o996zg2vmyx3396/user2.bin?dl=0";

http.createServer(function(request, response) {

  var uri = url.parse(request.url)
  console.log("request url: ", request.url);
  console.log("request header: ", request.headers);
  // console.log("uri = ", uri);

  if (uri.pathname == "/firmware/otaupdate/versions") {
    var image = request.headers.image;

    var fileUser = fileUser2;
    var retImage = 'user1.bin'
    if (image == 'user1.bin') {
      fileUser = fileUser2
      retImage = 'user2.bin'
    }
    else if (image == 'user2.bin') {
      fileUser = fileUser1
      retImage = 'user1.bin'
    }
    else {
      console.log("error");
    }

    // var urlUser = url.parse(fileUser);
    // var res = {
    //   application: "otaupdate",
    //   last:
    //     {
    //       version: "1.0.2",
    //       updated: 1430135590467,
    //       url: urlUser.path,
    //       protocol: urlUser.protocol,
    //       host: urlUser.hostname,
    //     }
    // }
    var res = {
      application: "otaupdate",
      last:
        {
          version: "1.0.2",
          updated: 1430135590467,
          url: retImage,
          protocol: 'http:',
          host: '192.168.1.178',
        }
    }
    response.write(JSON.stringify(res));
    response.end();
  }
  else {
    if (uri.pathname == "/user1.bin")
      filename = fileUser1;
    else if (uri.pathname == "/user2.bin")
      filename = fileUser2;
    else {
      response.writeHead(404)
      return response.end();
    }

    // testing
    // response.writeHead(404)
    // return response.end();
    // end testing

    console.log(filename)
  
    fs.exists(filename, function(exists) {
      if(!exists) {
        response.writeHead(404, {"Content-Type": "text/plain"});
        response.write("404 Not Found\n");
        response.end();
        return;
      }

      fs.readFile(filename, "binary", function(err, file) {
        if(err) {        
          response.writeHead(500, {"Content-Type": "text/plain"});
          response.write(err + "\n");
          response.end();
          return;
        }

        response.writeHead(200, {"Content-Type": "application/x-download", "Content-Length": file.length});
        response.write(file, "binary");
        response.end();
      });
    });
  } // end else
}).listen(parseInt(port, 10));

console.log("Static file server running at\n  => http://localhost:" + port + "/\nCTRL + C to shutdown");
