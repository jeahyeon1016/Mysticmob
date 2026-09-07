import struct, zipfile, math, xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path.cwd()
raw = ROOT.joinpath('end.stl').read_bytes()
n = struct.unpack_from('<I', raw, 80)[0]
triangles = [[struct.unpack_from('<3f', raw, 84+i*50+12+j*12) for j in range(3)] for i in range(n)]
v, f = [], []
def add(points):
    i=len(v); v.extend(points); return i
for t in triangles:
    if sum(p[1] for p in t)/3 < -12:
        i=add(t); f.append((i,i+1,i+2))
def ring(af,y):
    r=af/(2*math.cos(math.pi/6))
    return [(r*math.cos(math.pi/6+k*math.pi/3),y,r*math.sin(math.pi/6+k*math.pi/3)) for k in range(6)]
o0,o1=ring(18,-12),ring(18,8); i1,i0=ring(12.2,8),ring(12.2,2)
a=add(o0); b=add(o1); c=add(i1); d=add(i0)
for k in range(6):
    q=(k+1)%6
    f += [(a+k,a+q,b+q),(a+k,b+q,b+k),(b+k,b+q,c+q),(b+k,c+q,c+k),(c+k,d+k,d+q),(c+k,d+q,c+q)]
f += [(d,d+1,d+2),(d,d+2,d+3),(d,d+3,d+4),(d,d+4,d+5)]
# close the underside so slicers see one solid part
f += [(a,a+2,a+1),(a,a+3,a+2),(a,a+4,a+3),(a,a+5,a+4)]
ns='http://schemas.microsoft.com/3dmanufacturing/core/2015/02'
m=ET.Element('{%s}model'%ns,{'unit':'millimeter','xml:lang':'en-US'}); r=ET.SubElement(m,'{%s}resources'%ns); o=ET.SubElement(r,'{%s}object'%ns,{'id':'1','type':'model'}); me=ET.SubElement(o,'{%s}mesh'%ns); vs=ET.SubElement(me,'{%s}vertices'%ns)
for x,y,z in v: ET.SubElement(vs,'{%s}vertex'%ns,{'x':str(x),'y':str(y),'z':str(z)})
ts=ET.SubElement(me,'{%s}triangles'%ns)
for x,y,z in f: ET.SubElement(ts,'{%s}triangle'%ns,{'v1':str(x),'v2':str(y),'v3':str(z)})
bu=ET.SubElement(m,'{%s}build'%ns); ET.SubElement(bu,'{%s}item'%ns,{'objectid':'1'})
xml=ET.tostring(m,encoding='utf-8',xml_declaration=True)
ct=b'''<?xml version="1.0" encoding="UTF-8"?><Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/><Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/></Types>'''
rels=b'''<?xml version="1.0" encoding="UTF-8"?><Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"><Relationship Target="/3D/3dmodel.model" Id="r1" Type="http://schemas.microsoft.com/3d/2013/01/3dmodel"/></Relationships>'''
with zipfile.ZipFile(ROOT/'hex_hub_adapter.3mf','w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('[Content_Types].xml',ct); z.writestr('_rels/.rels',rels); z.writestr('3D/3dmodel.model',xml)
with open(ROOT/'hex_hub_adapter.stl','wb') as out:
    out.write(b'hex hub adapter'.ljust(80,b' ')); out.write(struct.pack('<I',len(f)))
    for a,b,c in f:
        p,q,r=v[a],v[b],v[c]
        out.write(struct.pack('<3f',0,0,0)+struct.pack('<9f',*(p+q+r))+struct.pack('<H',0))
print('created',ROOT/'hex_hub_adapter.3mf')
