# SimpleHttpServer
* Implement GET
* Implement 200, 400, 404
* Header check
* Message size limit free
* Socket reuse and intended connection close
* malware path prohibited

## todo
* report dependencies error when building, but doesn't affect execution
* may pass the check sometimes for malware path

## Build
```
make clean # if necessary
make
```

## Run server
```
./httpd myconfig.ini
```

## simple test using curl
```
curl -o test_img.jpg http://localhost:8080/kitten.jpg
```
```
./TCPClient localhost 8080
```
