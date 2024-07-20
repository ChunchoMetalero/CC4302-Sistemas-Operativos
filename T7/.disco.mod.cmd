cmd_/home/debian/Win/T7/disco.mod := printf '%s\n'   kmutex.o disco-impl.o | awk '!x[$$0]++ { print("/home/debian/Win/T7/"$$0) }' > /home/debian/Win/T7/disco.mod
