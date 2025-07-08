

* [x] check arguments
* [x] add signals, and init signal handler
* [x] open log
* [x] Create socket
* [x] Set socket
* [x] getaddressinfo
* [x] bind
* [x] deamo?
  * [x] fork
* [x] listen
 #
* ### Loop1:
  #### Until the kill signal is received.
  * [x] open socket data file in write mode
  * [x] accept
  #
  * ### Loop2:
    ### repeat until the end value is 0x0A
    * [x] Recieve the input data from the socket
    #
  * [x] Write recevied data to the data file.
  * [x] Open again the data file in a seperate File.
  * [x] Read the file completly
  * [x] send back the read data completly to the socket
#
* [x] Close all