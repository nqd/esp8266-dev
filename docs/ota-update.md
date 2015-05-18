# this document describes the method of ota firmware updating

When bootup, OTA client polls for latest informations of firmware of an application via REST calling:

    GET /firmware/:application/versions
    HEADERS:
        uuid: uuid
        token: token
        host: esp8266
        version: current version
        user: user1/user2

OTA server should return information of last version, which contain metadata of the latest, including URL to get user1/user2.bin.

    {
        application: application,
        last:
            {
                version: 0.1.100,
                created: timestamp,
                protocol: http/https,
                host: cdn-host,
                url: /firmware/:application/versions/0.1.100
            }
    }

Note: ESP host will refuse the response if it doesnot contain tuble {version, protocol, host, url}.

Get the raw image of an application at specific version

    Connect cdn-host
    GET /url HTTP/1.1

One when create new version of an application, need to register to fota server. The registration infomation provides dirrect url for the user1.bin and user2.bin, e.g. from Dropbox or Amazon.

    POST /firmware/:application
    HEADERS:
        token: token

    BODY
    {
        application: application,
        version: new version,
        host: esp8266
        firmwares: [{
            name: user1.bin,
            url: "https://dl.dropboxusercontent.com/s/jwnjhet4tngh3nh/user1.bin?dl=0"
        },
        {
            name: user2.bin,
            url: "https://dl.dropboxusercontent.com/s/o996zg2vmyx3396/user2.bin?dl=0"
        }]
    }

Note: the url provided should be the dirrect link to download userx.bin, since esp http client is simple, cannot handle redirect HTTP links. Use ```curl -I url``` to check for return which should contain Content-Length:
```
HTTP/1.1 200 OK
accept-ranges: bytes
cache-control: max-age=0
Content-Length: 387808
```

