import sys
f = open('nul', 'w')
sys.stderr = f
import gzip
uncompressedData = open(sys.argv[1], "rt", encoding='utf-8')
data = uncompressedData.read()
ocsFile = gzip.open(sys.argv[2], "wt")
ocsFile.write(data)
# compressedData = gzip.compress(bytes(data, 'utf-8'))
# sys.stdout.buffer.write(compressedData)