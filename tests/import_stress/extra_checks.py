import csv, io, json, subprocess, sys, hashlib
from pathlib import Path
import run_import_tests as t

def permissions():
    source=t.FIX/'minimal.vtp';denied=t.FIX/'deny-read.vtp';denied.write_bytes(source.read_bytes())
    identity=subprocess.check_output(['whoami','/user','/fo','csv','/nh'],encoding='utf-8').strip()
    sid=next(csv.reader(io.StringIO(identity)))[1]
    change=subprocess.run(['icacls',str(denied),'/deny','*'+sid+':(R)'],capture_output=True)
    if change.returncode:raise RuntimeError('Cannot establish denied-read fixture')
    try:
        try:denied.read_bytes()
        except PermissionError:pass
        else:raise RuntimeError('Denied-read fixture remains readable')
        t.manifest('permission-denied',[t.case('denied',denied,'reject'),t.case('recovery',source,points=3,cells=1)])
    finally:
        restore=subprocess.run(['icacls',str(denied),'/remove:d','*'+sid],capture_output=True)
        if restore.returncode:raise RuntimeError('Could not restore generated fixture ACL')

def larger():
    for size in (3000000000,4000000000,5000000000):
        label=str(size//1000000000)+'GB';p=t.ensure_sample(size)
        points,faces=t.metadata(p)
        h=hashlib.sha256()
        with p.open('rb') as f:
            for block in iter(lambda:f.read(8*1024*1024),b''):h.update(block)
        with (t.OUT/'limit-samples.jsonl').open('a',encoding='utf-8') as f:
            f.write(json.dumps(dict(label=label,path=str(p),bytes=p.stat().st_size,points=points,faces=faces,sha256=h.hexdigest()))+'\n')
        result=t.manifest('limit-'+label,[t.case(label,p,points=points,cells=faces,faces=faces)])
        if result['status']!='pass':break

def ui():
    p=t.ensure_sample(100000)
    t.guarded('ui-import-cancel-error-recovery',t.BIN/'ui_import_only.exe',[p,563,1122,1683],timeout=120)

def supplements():
    good=t.FIX/'minimal.vtp'
    distinct=[]
    for i in range(100):
        p=t.FIX/('distinct-'+str(i)+'.vtp');p.write_bytes(good.read_bytes())
        distinct.append(t.case('distinct-'+str(i),p,points=3,cells=1,faces=1))
    t.manifest('batch-distinct-100',distinct)
    t.manifest('empty-batch',[])
    for length in (260,261):
        parent=t.FIX/'long'
        while len(str(parent))+65<length:
            parent=parent/('segment_'+'x'*30);t.os.makedirs('\\\\?\\'+str(parent),exist_ok=True)
        p=parent/('x'*max(1,length-len(str(parent))-5)+'.vtp');t.shutil.copyfile(good,'\\\\?\\'+str(p))
        t.manifest('path-length-'+str(length),[t.case('long-path',p,'observe'),t.case('recovery',good,points=3,cells=1)])
    fixture=t.TOOLS/'fixtures/topology'
    for row in csv.DictReader((fixture/'manifest.csv').open(encoding='utf-8-sig')):
        good=row['expected_result']=='ok'
        t.manifest('topology-'+row['file'],[t.case(row['file'],fixture/row['file'],'accept' if good else 'reject',**({key:int(row[col]) for key,col in [('points','points'),('cells','cells'),('faces','expected_boundary_faces')]} if good else {}))])
    for name,content,expect in [
        ('negative-count',t.triangle(np=-1),'reject'),
        ('oversized-declared-count',t.triangle(np=2147483648),'reject'),
        ('invalid-offset',t.triangle(offsets='4'),'reject'),
        ('collinear-triangle',t.triangle(points='0 0 0 1 0 0 2 0 0'),'observe')]:
        p=t.FIX/(name+'.vtp');p.write_text(content,encoding='utf-8')
        t.manifest(name,[t.case(name,p,expect)])
    big=t.ensure_sample(2000000000)
    t.manifest('cancel-during-2GB-read',[t.case('cancelled',big,'reject')],cancelMs=50)
    t.manifest('destroy-during-2GB-read',[t.case('destroyed',big)],destroyActive=True,destroyDelayMs=50)

def recheck():
    t.guarded('oversized-declared-count-recheck',t.BIN/'import_cases.exe',[t.FIX/'oversized-declared-count.json'],budget=256,timeout=30)

if __name__=='__main__':
    {'permissions':permissions,'larger':larger,'ui':ui,'supplements':supplements,'recheck':recheck}[sys.argv[1]]()
