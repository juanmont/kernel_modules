 #!/bin/bash 
         CONTADOR=0
         while [  $CONTADOR -lt 20 ]; do
			echo Productor: 
			echo $CONTADOR &&  echo add $CONTADOR > /proc/modlist
			echo ---------
			cat /proc/modlist
			echo ----------
             sleep .5
             let CONTADOR=CONTADOR+1 
         done
