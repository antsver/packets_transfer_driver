# packets_transfer_driver
Driver for variable length packets transfer

### Description

- driver converts each application packet into frame with delimiters, byte stuffing and CRC 
- driver calls low level callbacks to send and receive bytes of frame over URAT or CAN interface
- application sends a packet calling driver's function and receives packet from callback called from driver

### Interfaces and dependencies

##### Driver has two interfaces:
  - upper level API to be called from application and one callback to be called from driver
  - low level callbacks to be called from driver to access hardware UART and CAN

##### Driver doesn't depend on certain hardware level or system level:

  - driver use simple callback functions to access hardware UART and CAN drivers
  - driver can be used either in bare metal system or within OS thread
  - hardware UART and CAN drivers can either use buffering (possibly with DMA) or not
  - diver doesn't require any time counters

##### Driver is supposed to be used with either CAN or UART:

  - hardware interface is selected using preprocessor directives
  - all hardware (clocks, GPIO, baudrate etc.) should configured before driver usage

##### Driver can be used in the multithreading environment

  - all functions are reenterable

  - driver doesn't use neither internal static data nor memory allocation

  - driver instance and all packet buffers are supposed to be stored externally, at the application level

### Framing and encoding

| Application level   | Frame level    |
|---------------------|----------------|
|                     | 0x7E           |
| PAYLOAD             | PAYLOAD (with byte stuffing)       |
|                     | CRC16 (with byte stuffing)         |
|                     | 0x7E           |

- Maximum payload size: configurable in runtime, during driver initialization 
- CRC-16-CCITT: poly 0x8408; init 0xFFFF; xor 0xFFFF; refIn true; refOut true; test 0x906E 
- Byte stuffing: 0x7E is 0x7D 0x5E and 0x7D is 0x7D 0x5D
- Low level communication:
  - UART sending: send all bytes of frame one-by-one
  - UART receiving: receive all bytes of frame one-by-one
  - CAN sending: send all bytes of frame within series of CAN messages (all messages have the same CAN ID which should be passed from application)
  - CAN receiving: receive series of CAN messages and extract frame from them (all messages should have the same CAN ID which should be passed from application)

