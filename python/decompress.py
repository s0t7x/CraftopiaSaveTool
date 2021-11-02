from sys import argv
from sys import stderr
f = open('nul', 'w')
stderr = f

import gzip
uncompressedData = gzip.open(argv[1], 'rt', encoding='utf-8')
data = uncompressedData.read()
print(data)