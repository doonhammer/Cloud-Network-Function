# REST API to Control VNF

Basic REST API to remotely control the smaple VNF

The following operations are supported:

* Start
* Restart
* Stop
* Status


# URL 

info

GET 	http:/hostname:port/vnf/info Returns the status of the VNF
POST	http:/hostname:port/vnf/info Restarts the VNF if already started otherwise starts VNF
PUT 	http:/hostname:port/vnf/info Starts the VNF if already started this will return an error
DELETE  http:/hostname:port/vnf/info Stops the VNF if not runnning returns an error


interface

GET 	http:/hostname:port/vnf/interface Returns all interfaces of the VNF
GET     http:/hostname:port/vnf/interface//id' Returns statistics of interface 'id'