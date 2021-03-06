# quakophon

Experimental audio recorder for anuran bioacoustics

Design goals:

* record stereo WAV files to SD card with 44.1ksps sample rate enabling high quality audio recordings from 20Hz to 20kHz
* automated recordings
* triggered recordings with adjustable threshold
* real time clock RTC
* time stamp on WAV files
* make directories for each day of recording and store files accordingly
* simultaneous logging of temperature with one or two waterproof temperature loggers (air temperature / water temperature)
* "waterproof" construction enabling recording under tropical field conditions
* scheduled audio logging
* hibernating during non-logging time frames --> BUT: in order to keep USB power bank happy and alive, wake-up every five minutes and draw current for one second or so
* low cost


Hardware:
* Teensy 3.6 microcontroller
* Teensy audio board
* electret microphone
* USB power bank
* waterproof small case
* Lithum battery for integrated real time clock
* SD-card 64GB
* waterproof push buttons
* waterproof temperature logger
