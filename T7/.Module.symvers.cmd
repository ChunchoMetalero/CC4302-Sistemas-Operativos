cmd_/home/debian/Win/T7/Module.symvers :=  sed 's/ko$$/o/'  /home/debian/Win/T7/modules.order | scripts/mod/modpost -m      -o /home/debian/Win/T7/Module.symvers -e -i Module.symvers -T - 
