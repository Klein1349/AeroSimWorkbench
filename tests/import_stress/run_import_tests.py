"""Import-only tests against the real BoundaryLoadService. Each run is job-limited."""
import argparse, datetime, hashlib, json, os, re, shutil, subprocess, sys, time
from pathlib import Path

TOOLS=Path(__file__).resolve().parent
ROOT=TOOLS.parents[1]
OUT=Path(os.environ.get('AEROSIM_IMPORT_TEST_OUTPUT',str(ROOT/'out'/('import-stress-'+datetime.datetime.now().strftime('%Y%m%d-%H%M%S'))))).resolve()
OUT.mkdir(parents=True,exist_ok=True)
STACK=Path(os.environ['AEROSIM_LOCAL_STACK']).resolve()
BIN=Path(os.environ.get('AEROSIM_IMPORT_TEST_BIN',str(ROOT/'build-import-stress/Debug'))).resolve()
GENERATOR=OUT/'generator/BoundaryMesh.exe'
SAMPLES=Path(os.environ.get('AEROSIM_IMPORT_SAMPLES',str(OUT/'samples'))).resolve()
FIX=OUT/'fixtures'; LOG=OUT/'logs'; RUN=OUT/'runtime'
for p in (FIX,LOG,RUN): p.mkdir(exist_ok=True)
env=os.environ.copy();qt=STACK/'deps/qt/5.12.12/msvc2017_64'
env.update(PATH=';'.join(map(str,[STACK/'v/bin',STACK/'deps/cgns-4.4.0-debug/bin',qt/'bin',env['PATH']])),
           QT_PLUGIN_PATH=str(qt/'plugins'),QT_QPA_PLATFORM_PLUGIN_PATH=str(qt/'plugins/platforms'),
           TMP=str(RUN),TEMP=str(RUN),AEROSIM_PROBE_SETTINGS=str(RUN))

def guarded(name,exe,args,budget=6144,timeout=300):
    if '--resume' in sys.argv and (OUT/'results.jsonl').exists():
        prior=[json.loads(line) for line in (OUT/'results.jsonl').read_text(encoding='utf-8').splitlines()]
        match=next((r for r in prior if r['name']==name),None)
        if match:return match
    cmd=[sys.executable,str(TOOLS/'run_guarded.py'),'--cwd',str(RUN),
         '--max-private-mib',str(budget),'--timeout-sec',str(timeout),'--',str(exe),*map(str,args)]
    start=time.time();print('START '+name,flush=True)
    with (LOG/(name+'.stdout.jsonl')).open('w',encoding='utf-8') as out,(LOG/(name+'.stderr.log')).open('w',encoding='utf-8') as err:
        result=subprocess.run(cmd,env=env,stdout=out,stderr=err,timeout=timeout+30)
    records=[]
    for line in (LOG/(name+'.stdout.jsonl')).read_text(encoding='utf-8').splitlines():
        try: records.append(json.loads(line))
        except json.JSONDecodeError: pass
    guard=next((r for r in records if r.get('record')=='guard'),{})
    status='pass' if result.returncode==0 else ('resource_limited' if guard.get('status') in ('job_memory_limit','host_physical_reserve','host_commit_reserve','insufficient_host_headroom') else 'fail')
    summary=dict(name=name,status=status,exitCode=result.returncode,wallSeconds=round(time.time()-start,3),records=records)
    with (OUT/'results.jsonl').open('a',encoding='utf-8') as f:f.write(json.dumps(summary,ensure_ascii=False)+'\n')
    print(json.dumps({k:v for k,v in summary.items() if k!='records'},ensure_ascii=True),flush=True)
    return summary

def case(name,path,expect='accept',**kw):return dict(name=name,path=str(path),expect=expect,**kw)
def manifest(name,cases,**kw):
    p=FIX/(name+'.json');p.write_text(json.dumps(dict(cases=cases,**kw),ensure_ascii=False),encoding='utf-8')
    return guarded(name,BIN/'import_cases.exe',[p],timeout=kw.pop('_timeout',300))

def triangle(points='0 0 0 1 0 0 0 1 0',ids='0 1 2',np=3,nf=1,offsets='3',pd=''):
    return f'''<?xml version="1.0"?><VTKFile type="PolyData" version="0.1" byte_order="LittleEndian"><PolyData><Piece NumberOfPoints="{np}" NumberOfPolys="{nf}" NumberOfVerts="0" NumberOfLines="0" NumberOfStrips="0"><PointData>{pd}</PointData><CellData/><Points><DataArray type="Float64" NumberOfComponents="3" format="ascii">{points}</DataArray></Points><Polys><DataArray type="Int64" Name="connectivity" format="ascii">{ids}</DataArray><DataArray type="Int64" Name="offsets" format="ascii">{offsets}</DataArray></Polys></Piece></PolyData></VTKFile>'''

def boundary():
    good=FIX/'minimal.vtp';good.write_text(triangle(),encoding='utf-8')
    recovery=case('valid-after-test',good,points=3,cells=1,faces=1)
    for p in sorted((ROOT/'examples/preprocess/meshes').glob('*')):
        if p.suffix.lower() not in ('.vtk','.vtp','.vtu','.vti','.cgns'):continue
        pyramid=p.stem=='triangular_pyramid';pts=4 if pyramid else 8;faces=4 if pyramid else 6
        manifest('format-'+p.name,[case(p.name,p,points=pts,cells=faces if p.suffix=='.vtp' else 1,faces=faces)])
    variants={
      'minimal':(triangle(),'accept'), 'empty':('','reject'), 'header-only':('<?xml version="1.0"?>','reject'),
      'zero-points':(triangle(points='',np=0),'reject'), 'zero-cells':(triangle(ids='',offsets='',nf=0),'reject'),
      'out-of-range':(triangle(ids='0 1 3'),'reject'), 'negative-index':(triangle(ids='0 1 -1'),'reject'),
      'duplicate-node':(triangle(ids='0 1 1'),'reject'), 'duplicate-face':(triangle(ids='0 1 2 0 1 2',nf=2,offsets='3 6'),'reject'),
      'missing-coordinates':(triangle().replace('0 0 0 1 0 0 0 1 0',''),'reject'),
      'truncated':(triangle()[:240],'reject'), 'nan-coordinate':(triangle(points='nan 0 0 1 0 0 0 1 0'),'reject'),
      'short-field':(triangle(pd='<DataArray type="Float64" Name="Pressure" format="ascii">1 2</DataArray>'),'reject'),
      'reserved-field':(triangle(pd='<DataArray type="Int64" Name="vtkOriginalPointIds" format="ascii">0 1 2</DataArray>'),'reject'),
      'unsupported':(triangle(),'reject')}
    for name,(content,expect) in variants.items():
        p=FIX/(name+('.xyz' if name=='unsupported' else '.vtp'));p.write_text(content,encoding='utf-8')
        manifest('content-'+name,[case(name,p,expect),recovery])
    for name,ext,expect in [('uppercase','.VTP','accept'),('xml-as-legacy','.vtk','reject'),('poly-as-vtu','.vtu','observe'),('no-extension','','reject')]:
        p=FIX/(name+ext);shutil.copyfile(good,p);manifest('extension-'+name,[case(name,p,expect),recovery])
    for name in ['中文 空格网格','spaces and (brackets) # &','emoji-🚀']:
        p=FIX/(name+'.vtp');shutil.copyfile(good,p)
        manifest('path-'+('unicode-emoji' if 'emoji' in name else 'chinese' if '中文' in name else 'special'),[case(name,p,'observe' if 'emoji' in name else 'accept'),recovery])
    for length in (240,259,280,400):
        parent=FIX/'long';parent.mkdir(exist_ok=True)
        while len(str(parent))+65<length:
            parent=parent/('segment_'+'x'*30);os.makedirs('\\\\?\\'+str(parent),exist_ok=True)
        p=parent/('x'*max(1,length-len(str(parent))-5)+'.vtp');shutil.copyfile(good,'\\\\?\\'+str(p))
        manifest('path-length-'+str(len(str(p))),[case('long-path',p,'observe'),recovery])
    manifest('missing-file',[case('missing',FIX/'absent.vtp','reject'),recovery])
    p=FIX/'locked.vtp';shutil.copyfile(good,p)
    manifest('exclusive-lock',[case('locked',p,'reject'),recovery],lock=str(p))
    for op in ('delete','move','truncate'):
        p=FIX/('queued-'+op+'.vtp');shutil.copyfile(good,p)
        manifest('queued-'+op,[recovery,case(op,p,'reject'),recovery],mutate=dict(path=str(p),op=op))
    for n in (1,10,50,100):manifest('batch-'+str(n),[case('item-'+str(i),good,points=3,cells=1,faces=1) for i in range(n)])
    same=[]
    for i in range(2):
        p=FIX/('folder-'+str(i))/'same.vtp';p.parent.mkdir(exist_ok=True);p.write_text(triangle(),encoding='utf-8');same.append(case('same-name-'+str(i),p,points=3,cells=1))
    manifest('same-name-different-paths',same)
    guarded('repeat-small-100',BIN/'lifecycle_probe.exe',[good,3,1,100])

def metadata(path):
    with path.open('rb') as f:head=f.read(4096)
    return int(re.search(rb'NumberOfPoints="(\d+)"',head)[1]),int(re.search(rb'NumberOfPolys="(\d+)"',head)[1])

def ensure_sample(size):
    label=str(size//1000000000)+'GB' if size%1000000000==0 else str(size)+'B'
    path=SAMPLES/('boundary-'+label+'.vtp')
    if not path.exists():
        result=subprocess.run([str(GENERATOR),str(SAMPLES),str(size)],capture_output=True,text=True,timeout=180)
        print(result.stdout,flush=True)
        if result.returncode:raise RuntimeError(result.stderr)
    return path

def stress():
    samples=[(label,ensure_sample(size)) for label,size in [('small',100000),('100MB',100000000),('500MB',500000000),('1GB',1000000000),('2GB',2000000000)]]
    inventory=[]
    expected={'1GB':'5440fb6437ccb04b502f3409d7f52003f50fb0d72ab7821b6c49767545e1b15a','2GB':'0df1243998bd4f09e2b7d1b4014e7fd84a97cd37ce03ec6dfffca3ec67295cf8'}
    for label,p in samples:
        h=hashlib.sha256()
        with p.open('rb') as f:
            for block in iter(lambda:f.read(8*1024*1024),b''):h.update(block)
        if label in expected and h.hexdigest()!=expected[label]:raise RuntimeError('Fixture hash mismatch: '+label)
        points,faces=metadata(p);inventory.append(dict(label=label,path=str(p),bytes=p.stat().st_size,points=points,faces=faces,sha256=h.hexdigest()))
        print('VERIFIED '+label,flush=True)
    (OUT/'samples.json').write_text(json.dumps(inventory,indent=2,ensure_ascii=False),encoding='utf-8')
    for item in inventory:
        c=case(item['label'],item['path'],points=item['points'],cells=item['faces'],faces=item['faces'])
        for n in range(1,4):manifest('stress-'+item['label']+'-'+str(n),[c])
    for item in inventory[-2:]:
        guarded('repeat-'+item['label']+'-10',BIN/'lifecycle_probe.exe',[item['path'],item['points'],item['faces'],10],timeout=600)
        manifest('destroy-active-'+item['label'],[case(item['label'],item['path'])],destroyActive=True)
    item=inventory[-1]
    for count in (2,3):
        result=manifest('capacity-2GB-x'+str(count),[case('mesh-'+str(i),item['path'],points=item['points'],cells=item['faces']) for i in range(count)])
        if result['status']!='pass':break

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('phase',choices=['boundary','stress']);p.add_argument('--resume',action='store_true');args=p.parse_args()
    (boundary if args.phase=='boundary' else stress)()
