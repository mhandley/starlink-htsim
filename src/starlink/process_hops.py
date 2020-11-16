file = open("hops", "r")
prevhop = -1
disttotal = 0
prevtime = "0"
for line in file:
    #print(line)
    parts = line.split(" ")
    #print(parts)
    time = parts[1]
    hop = int(parts[3])
    dist = float(parts[7])
    if hop < prevhop:
        print("time " + prevtime + " dist " + str(disttotal))
        disttotal = 0
        prevtime = time
    disttotal += dist
    prevhop = hop
    
