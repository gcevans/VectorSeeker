#!/usr/bin/python
import sys
import cgi
import os
import errno

def pairsCompress(b,a):
	lastsize = b[0]
	count = b[1]
	instr = b[2]
	if lastsize == -1:
		return (a[1],1,'<{0},{1}>'.format(a[0],a[1]))
	if a[1] == lastsize:
		count += 1
		return (lastsize,count,b[2])
	else:
		lastsize = a[1]
		if count > 1:
			strvalue = instr+'...x{0},<{1},{2}>'.format(count,a[0],a[1])
		else:
			strvalue = instr+',<{0},{1}>'.format(a[0],a[1])
		return (lastsize, 1, strvalue)

def writeHeader(f,title):
	f.write('<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.1//EN" "http://www.w3.org/TR/xhtml11/DTD/xhtml11.dtd">\n')
	f.write('<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en"><head>\n')
	f.write('<meta http-equiv="content-type" content="text/html; charset=UTF-8"/>\n')
	f.write('<title>'+title+'</title>\n')
	f.write('<style type="text/css">\n')
	f.write('.lnumb { color: black;}\n')
	f.write('.lunchecked { color: black;}\n')
	f.write('.lvec { color: green;}\n')
	f.write('.lvec A:link {background:#00FF00;}\n')
	f.write('.lvec A:visited { background:#00FF00;}\n')
	f.write('.lpart { color: yellow;}\n')
	f.write('.lpart A:link {background:#FFFF00;}\n')
	f.write('.lpart A:visited {background:#FFFF00;}\n')
	f.write('.lnot { color: red;}\n')
	f.write('.lnot A:link {background:#FF0000;}\n')
	f.write('.lnot A:visited {background:#FF0000;}\n')
	f.write('il, div { border: 1px; margin: 1px;}\n')
	f.write('div:target {border: 2px solid gray; margin: 1px;}\n')
	f.write('body {font-family:"Courier New", Courier, monospace;  font-size: 80%}\n')
	f.write('</style>\n')
	f.write('</head>\n')
	f.write('<body>\n')

def writeFooter(f):
	f.write('</body>\n')
	f.write('</html>\n')
	
def numbers2pair(s):
	numbs = s.split(',')
	return (int(numbs[0]),int(numbs[1]))
	
def whitespacer(s):
	return s.replace(' ','&nbsp')

if len(sys.argv) < 2 or len(sys.argv) > 5:
	print("Usage: logparse.py inputfile [outputdir]")
	sys.exit()
	
if len(sys.argv) > 2:
	outpath = os.path.realpath(sys.argv[2])
	try:
		os.makedirs(outpath)
	except OSError, e:
		if e.errno != errno.EEXIST:
			raise
else:
	outpath = os.path.realpath('.')

if len(sys.argv) > 3:
	minvectorwidth = int(sys.argv[3])
else:
	minvectorwidth = 100
	
if len(sys.argv) > 4:
	objfilename = sys.argv[4]
	outobjfilename = outpath + '/objectfile.html'
else:
	objfilename = ''

logfilename = sys.argv[1]

try:
	f=open(logfilename,'r')
except IOError:
	print 'can not open file ', sys.argv[1]
else:
	detailname = outpath+'/log.html'
	detailfile = open(detailname,'w')
	writeHeader(detailfile,logfilename + ' details')
	codefilesset = set()
	codefiledict = dict()
	inlocations = []
	locationid = 0
	instruction = 0
	idinfo = dict()
	vectoraddrset = set()
	novectoraddrset = set()

# Build Indexes and write log file
	for line in f:
		if not line.startswith('\t'):
			instruction = 0
			locationid += 1
			if locationid > 1:
				detailfile.write('</div>\n</div>\n')
			detailfile.write('<div id="id{0}">\n<div>'.format(locationid)+line+'</div>\n')
			filename = line.split(',')[0]
			linenumber = line.split(',')[1].split(':')[0]
			execount = line.split(',')[1].split(':')[1]
			location = (filename,linenumber,locationid,execount)
			inlocations.append(location)
			idinfo[locationid] = []
			if filename in codefiledict:
				codefiledict[filename][linenumber] = locationid
			else:
				codefiledict[filename] = {linenumber:locationid}
			codefilesset.add(filename)
		else:
			if line.startswith('\t0'):
				instruction += 1
				if instruction > 1:
					detailfile.write('</div>\n')
				currentid = line.split(':')[0].split('x')[1]
				if objfilename != '':
					addr=line.lstrip().split(':')[0]
					detailfile.write('<div id="addr'+addr+'" style="margin-left: 2em;">\n<a href="'+outobjfilename+'#addr'+addr+'">'+line.strip()+'</a>\n')
				else:
					detailfile.write('<div style="margin-left: 2em;">\n'+line+'</div>')
			else:
				pairs = map(numbers2pair,line.strip('\t \n<>').split('><'))
				maxwidth = max(map(lambda x: x[1], pairs))
				pairs.sort(lambda x,y: int(x[0]) - int(y[0]))
				pairs.sort(lambda x,y: int(y[1]) - int(x[1]))
#				printpairs = map(lambda x: '<{0},{1}>'.format(x[0],x[1]), pairs)
				initval = (-1,1,'')
				(lastsize, count, printstring) = reduce(pairsCompress, pairs, initval)
				if count > 1:
					printstring = printstring + '...x{0}'.format(count)
				idinfo[locationid].append(maxwidth >= minvectorwidth)
				if maxwidth >= minvectorwidth:
					detailfile.write('<div style="margin-left: 2em;">\n<span class="lvec" style="white-space:nowrap;">')
					vectoraddrset.add(currentid)
				else:
					detailfile.write('<div style="margin-left: 2em;">\n<span class="lnot" style="white-space:nowrap;">')
					novectoraddrset.add(currentid)
#				detailfile.write(cgi.escape(''.join(printpairs).strip(),True))
				detailfile.write(cgi.escape(printstring,True))
				detailfile.write('</span>\n</div>\n')
	detailfile.write('</div>\n</div>\n')
	writeFooter(detailfile)
	detailfile.close()

# Strip shared prefixes from paths and locate in output directory
	codfiles = list(codefilesset)
	pathprefix = os.path.commonprefix(codfiles)
	locations = []
	for location in inlocations:
		filename = outpath+'/'+location[0][len(pathprefix):]
	#tuple is (outputfilename,linenumber,locationid,execount)
		locations.append( (filename,location[1],location[2],location[3]) )

# Wrie index file
	indexfile = open(outpath+'/index.html','w')
	writeHeader(indexfile,logfilename)
	indexfile.write('<ul>\n')
	for location in locations:
		if reduce(lambda x,y: x and y, idinfo[location[2]], True):
			indexfile.write('<li><span class="lvec"><a href="'+location[0]+'.html#l'+location[1]+'">V'+location[3]+': '+location[0]+','+location[1]+'</a></span></li>\n')
		elif reduce(lambda x,y: x or y, idinfo[location[2]], False):
			indexfile.write('<li><span class="lpart"><a href="'+location[0]+'.html#l'+location[1]+'">P'+location[3]+': '+location[0]+','+location[1]+'</a></span></li>\n')
		else:			
			indexfile.write('<li><span class="lnot"><a href="'+location[0]+'.html#l'+location[1]+'">N'+location[3]+': '+location[0]+','+location[1]+'</a></span></li>\n')
	indexfile.write('</ul>\n')
	if objfilename != '':
		indexfile.write('<p>Object file: <a href="'+outobjfilename+'">'+objfilename+'</a></p>\n')
	else:
		indexfile.write('<p>No object file</p>\n')
	writeFooter(indexfile)
	indexfile.close()

# Write Object file
	if objfilename != '':
		section = 0
		label = 0
		objfile = open(objfilename,'r')
		outobjfile = open(outobjfilename,'w')
		writeHeader(outobjfile,objfilename)
		for line in objfile:
			if line.startswith('  '):
				addr =  line.lstrip().split(':')[0]
				outobjfile.write('<div id="addr0x'+addr+'" style="margin-left: 2em;">')
				if addr in novectoraddrset:
					outobjfile.write('<span class="lnot">')
				elif addr in vectoraddrset:
					outobjfile.write('<span class="lvec">')
				else:
					outobjfile.write('<span class="lunchecked">')
				outobjfile.write(whitespacer(cgi.escape(line,True))+'</span></div>')
			elif line.startswith('/'):
				targetfile = line.split(':')[0]
				targetfile = targetfile[len(pathprefix):]
				targetfile = outpath+'/'+targetfile
				targetline = line.strip().split(':')[1]
				outobjfile.write('<div><a href="'+targetfile+'.html#l'+targetline+'">'+whitespacer(cgi.escape(line,True))+'</a></div>')
			elif line.startswith('Disassembly'):
				if section > 0:
					outobjfile.write('</div>')
				if label > 0:
					outobjfile.write('</div>')
					label = 0
				section += 1
				outobjfile.write('<div style="margin-top: 2em">'+whitespacer(cgi.escape(line,True)))
			elif line[0].isdigit():
				if label > 0:
					outobjfile.write('</div>')
				label += 1
				outobjfile.write('<div style="margin-top: 1em">'+whitespacer(cgi.escape(line,True)))
			else:
				outobjfile.write('<div>'+whitespacer(cgi.escape(line,True))+'</div>')
		outobjfile.write('\n</div>')
		writeFooter(outobjfile)
		outobjfile.close()
		objfile.close()

# Write Codefiles
	for file in codfiles:
		f = open(file,'r')
		outfilename = outpath+'/'+file[len(pathprefix):]+'.html'
		out = open(outfilename,'w')
		writeHeader(out,file)
		lineNumber = 1
		for line in f:
			if str(lineNumber) in codefiledict[file]:
				id = codefiledict[file][str(lineNumber)]
				if reduce(lambda x,y: x and y, idinfo[id], True):
					escapedline = '<div id="l{0}">\n<span class="lvec">{1:05} '.format(lineNumber,lineNumber)+'<a href="'+detailname+'#id{0}">'.format(id)+cgi.escape(line,True)+'</a></span>'
				elif reduce(lambda x,y: x or y, idinfo[id], False):
					escapedline = '<div id="l{0}">\n<span class="lpart">{0:05} '.format(lineNumber,lineNumber)+'<a href="'+detailname+'#id{0}">'.format(id)+cgi.escape(line,True)+'</a></span>'
				else:			
					escapedline = '<div id="l{0}">\n<span class="lnot">{0:05} '.format(lineNumber,lineNumber)+'<a href="'+detailname+'#id{0}">'.format(id)+cgi.escape(line,True)+'</a></span>'
			else:
				escapedline = '<div id="l{0}">\n<span class="lnumb">{0:05} </span>'.format(lineNumber,lineNumber)+cgi.escape(line,True)
			lineNumber += 1
			out.write(escapedline+'</div>\n')
		writeFooter(out)
		out.close()
