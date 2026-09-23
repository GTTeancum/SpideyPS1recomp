#!/usr/bin/env python3
"""PL_MPEG MPEG-1 VLC trees, compact transcription of its MIT-licensed tables.
See SOURCES.md. nN denotes a branch to row N, vN a decoded value, x invalid.
This script is only a build-time source-maintenance utility, not a media converter.
"""
from pathlib import Path
TABLES = {
 'Address': '''n1,v1 n2,n3 n4,n5 v3,v2 n6,n7 v5,v4 n8,n9 v7,v6 n10,n11 n12,n13 n14,n15 n16,n17 n18,n19 v9,v8 x,n20 x,n21 n22,n23 v15,v14 v13,v12 v11,v10 n24,n25 n26,n27 n28,n29 n30,n31 n32,x x,n33 n34,n35 n36,n37 n38,n39 v21,v20 v19,v18 v17,v16 v35,x x,v34 v33,v32 v31,v30 v29,v28 v27,v26 v25,v24 v23,v22''',
 'TypeI': '''n1,v1 x,v17''',
 'TypeP': '''n1,v10 n2,v2 n3,v8 n4,n5 n6,v18 v26,v1 x,v17''',
 'TypeB': '''n1,n2 n3,n4 v12,v14 n5,n6 v4,v6 n7,n8 v8,v10 n9,n10 v30,v1 x,v17 v22,v26''',
 'Pattern': '''n1,n2 n3,n4 n5,n6 n7,n8 n9,n10 n11,n12 n13,v60 n14,n15 n16,n17 n18,n19 n20,n21 n22,n23 v32,v16 v8,v4 n24,n25 n26,n27 n28,n29 n30,n31 v62,v2 v61,v1 v56,v52 v44,v28 v40,v20 v48,v12 n32,n33 n34,n35 n36,n37 n38,n39 n40,n41 n42,n43 v63,v3 v36,v24 n44,n45 n46,n47 n48,n49 n50,n51 n52,n53 n54,n55 n56,n57 n58,n59 v34,v18 v10,v6 v33,v17 v9,v5 x,n60 n61,n62 v58,v54 v46,v30 v57,v53 v45,v29 v38,v26 v37,v25 v43,v23 v51,v15 v42,v22 v50,v14 v41,v21 v49,v13 v35,v19 v11,v7 v39,v27 v59,v55 v47,v31''',
 'Motion': '''n1,v0 n2,n3 n4,n5 v1,v-1 n6,n7 v2,v-2 n8,n9 v3,v-3 n10,n11 n12,n13 x,n14 n15,n16 n17,n18 v4,v-4 x,n19 n20,n21 v7,v-7 v6,v-6 v5,v-5 n22,n23 n24,n25 n26,n27 n28,n29 n30,n31 n32,n33 v10,v-10 v9,v-9 v8,v-8 v16,v-16 v15,v-15 v14,v-14 v13,v-13 v12,v-12 v11,v-11''',
 'DcY': '''n1,n2 v1,v2 n3,n4 v0,v3 v4,n5 v5,n6 v6,n7 v7,n8 v8,x''',
 'DcC': '''n1,n2 v0,v1 v2,n3 v3,n4 v4,n5 v5,n6 v6,n7 v7,n8 v8,x''',
 'Coeff': '''n1,v0x0001 n2,n3 n4,n5 n6,v0x0101 n7,n8 n9,n10 v0x0002,v0x0201 n11,n12 n13,n14 n15,v0x0003 v0x0401,v0x0301 n16,v0xffff n17,n18 v0x0701,v0x0601 v0x0102,v0x0501 n19,n20 n21,n22 v0x0202,v0x0901 v0x0004,v0x0801 n23,n24 n25,n26 n27,n28 n29,n30 v0x0d01,v0x0006 v0x0c01,v0x0b01 v0x0302,v0x0103 v0x0005,v0x0a01 n31,n32 n33,n34 n35,n36 n37,n38 n39,n40 n41,n42 n43,n44 n45,n46 v0x1001,v0x0502 v0x0007,v0x0203 v0x0104,v0x0f01 v0x0e01,v0x0402 n47,n48 n49,n50 n51,n52 n53,n54 n55,n56 n57,n58 n59,n60 n61,n62 x,n63 n64,n65 n66,n67 n68,n69 n70,n71 n72,n73 n74,n75 n76,n77 v0x000b,v0x0802 v0x0403,v0x000a v0x0204,v0x0702 v0x1501,v0x1401 v0x0009,v0x1301 v0x1201,v0x0105 v0x0303,v0x0008 v0x0602,v0x1101 n78,n79 n80,n81 n82,n83 n84,n85 n86,n87 n88,n89 n90,n91 v0x0a02,v0x0902 v0x0503,v0x0304 v0x0205,v0x0107 v0x0106,v0x000f v0x000e,v0x000d v0x000c,v0x1a01 v0x1901,v0x1801 v0x1701,v0x1601 n92,n93 n94,n95 n96,n97 n98,n99 n100,n101 n102,n103 v0x001f,v0x001e v0x001d,v0x001c v0x001b,v0x001a v0x0019,v0x0018 v0x0017,v0x0016 v0x0015,v0x0014 v0x0013,v0x0012 v0x0011,v0x0010 n104,n105 n106,n107 n108,n109 n110,n111 v0x0028,v0x0027 v0x0026,v0x0025 v0x0024,v0x0023 v0x0022,v0x0021 v0x0020,v0x010e v0x010d,v0x010c v0x010b,v0x010a v0x0109,v0x0108 v0x0112,v0x0111 v0x0110,v0x010f v0x0603,v0x1002 v0x0f02,v0x0e02 v0x0d02,v0x0c02 v0x0b02,v0x1f01 v0x1e01,v0x1d01 v0x1c01,v0x1b01'''
}
def generate():
 out=['// Generated from make_vlc_tables.py. PL_MPEG / Dominic Szablewski, MIT.\n// See SOURCES.md and LICENSE-PL_MPEG.txt.\nstruct Vlc { int next; int value; };\n']
 for name,s in TABLES.items():
  rows=s.split();out.append(f'static const Vlc {name}[] = {{')
  for i,r in enumerate(rows):
   cells=[]
   for t in r.split(','):
    if t=='x':v=(-1,0)
    elif t.startswith('n'):
     j=int(t[1:]);assert 0<j<len(rows);v=(2*j,0)
    else:assert t.startswith('v');v=(0,int(t[1:],0))
    cells.append('{%d,%d}'%v)
   assert len(cells)==2
   out.append(' '+','.join(cells)+f', // {i}')
  out.append('};')
 return '\n'.join(out)+'\n'
if __name__=='__main__':Path(__file__).with_name('mpeg1_vlc.h').write_text(generate())
