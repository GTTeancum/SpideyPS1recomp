"""Compare a registered repaired FBX against an independently repeated source export."""
import argparse
import json
from pathlib import Path
import numpy as np
from scene import Scene

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--registered',type=Path,required=True);p.add_argument('--repeated',type=Path,required=True)
p.add_argument('--out',type=Path,required=True)
a=p.parse_args();left=Scene(a.registered);right=Scene(a.repeated)
checks={name:bool(np.array_equal(getattr(left,name),getattr(right,name))) for name in
        ('vertices','faces','uv','corner_normals','world_vertices','world_normals','mesh_bind','source_bind')}
checks['hierarchy']=left.names==right.names and left.parents==right.parents
checks['weights']=[sorted(row) for row in left.weights]==[sorted(row) for row in right.weights]
checks['sourceProperties']=left.source_properties==right.source_properties
checks['clusters']=sorted(left.clusters,key=lambda c:c['bone'])==sorted(right.clusters,key=lambda c:c['bone'])
report=dict(status='PASS' if all(checks.values()) else 'FAIL',registeredSha256=left.sha256,
            repeatedSha256=right.sha256,checks=checks,
            scope='Semantic equality of registered and repeated repaired exports; export timestamps/FBX IDs may differ.')
a.out.parent.mkdir(parents=True,exist_ok=True);a.out.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
raise SystemExit(0 if report['status']=='PASS' else 1)
