#!/usr/bin/env python3
"""Descarga LVGL-6.1.1-MKS (la misma version que usa el firmware) archivo por archivo desde raw.githubusercontent.com.
Uso: cd <carpeta destino>/lvgl && python3 fetch_lvgl.py"""
import re,os,sys,urllib.request,posixpath
B="https://raw.githubusercontent.com/makerbase-mks/LVGL-6.1.1-MKS/a3ebe98bc6/"
seen={}
def get(p):
    if p in seen: return seen[p]
    try:
        d=urllib.request.urlopen(B+p,timeout=20).read().decode('utf8','replace')
    except Exception as e:
        seen[p]=None; return None
    os.makedirs(os.path.dirname(p) or '.',exist_ok=True); open(p,'w').write(d); seen[p]=d; return d
q=['lvgl.h','lv_conf.h']
done=set()
while q:
    p=q.pop()
    if p in done: continue
    done.add(p)
    d=get(p)
    if d is None: continue
    base=posixpath.dirname(p)
    if p.endswith('.h') and p.startswith('src/'):
        c=p[:-2]+'.c'
        q.append(c)
    for m in re.finditer(r'#\s*include\s+"([^"]+)"',d):
        inc=m.group(1)
        for cand in (posixpath.normpath(posixpath.join(base,inc)),inc,'src/'+inc.lstrip('./')):
            if get(cand) is not None:
                q.append(cand); break
print(len([k for k,v in seen.items() if v]),'files ok;', [k for k,v in seen.items() if v is None and k.endswith('.h')][:20])
