# this document describes the method of ota firmware updating

API, the restful way

### ESP host side

When bootup, host report its version.

OTA client gets latest informations of firmware of an application via REST calling:

    GET /firmware/:application/versions
    HEADERS:
        uuid: uuid
        token: token
        host: esp
        version: current version

OTA server should return information of last version, which contain metadata of the latest, including URL to get user1/user2.bin.

    {
        application: application,
        last:
            {
                version: 0.1.100,
                created: timestamp,
                host: cdn-host,
                url: /firmware/:application/versions/0.1.100
            }
    }
<!--    histories: [
            {
                version:
                created:
                host: cdn-host
                url:
            },
        ]
-->    

Note: should return the last version only, to save ESP resources.

Get the raw image of an application at specific version

    Connect CDN-Host HTTP/1.1
    GET /firmware/:application/versions/0.1.100/user1.bin(user2.bin)
    HEADERS:
        UUID: uuid
        token: token
        host: esp               // FIXED value
        version: current version

### Developing site

Should follow http://semver.org/ for version labeling.

    MAJOR version when you make incompatible API changes,
    MINOR version when you add functionality in a backwards-compatible manner, and
    PATCH version when you make backwards-compatible bug fixes.

Additional labels for pre-release and build metadata are available as extensions to the MAJOR.MINOR.PATCH format. 

Use POST to create new version. User should push 2 images, user1.bin and user2.bin in BSON. If BODY.version existed, and server accept this suggested version, it should be the latest version. Otherwise, type will be used to generate next version.

    POST /firmware/:application/versions
    HEADERS:
        authentication: 
    BODY
    {
        version: suggested_version,         // suggested by host
        type: major/minor/patch,
        user1: raw_image,
        user2: raw_image
    } // bson format

should return

    BODY
    {
        version: new version,
        timestamp: unix timestamp
        url: /firmware/:application/versions/0.1.100
    }
