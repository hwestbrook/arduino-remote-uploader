#!/bin/bash

/etc/alternatives/java -d32 -Djava.library.path=. -classpath "*" com.rapplogic.aru.uploader.xbee.XBeeSketchUploader "$@"
