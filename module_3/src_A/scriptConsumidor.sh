 #!/bin/bash 
         CONTADOR=0
         while [  $CONTADOR -lt 20 ]; do
			echo remove $CONTADOR > /proc/modlist
             sleep 1
             let CONTADOR=CONTADOR+1 
         done
