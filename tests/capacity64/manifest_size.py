"""Tamano del manifiesto ARQUIS_OBJECTS_V1 (borrador, shared/object_protocol/ARQUIS_OBJECTS_V1.md) con valores de longitud maxima."""
def manifest(n, names):
    L = [";ARQUIS_OBJECTS_BEGIN", ";ARQ:H V=1 PID=3FA9C21B N=%d BED=3000,3000 MIN=0,0 ORG=FL U=0.1mm CAP=64" % n]
    L += [";ARQ:M %d %s" % (i, nm) for i, nm in enumerate(names)]
    L += [";ARQ:O %d %d %d 2999 2999 3000 3000 359 R 4500" % (i, i % len(names), i) for i in range(n)]
    L += [";ARQUIS_OBJECTS_END CRC=1A2B3C4D", ";ARQUIS_TRAILER OFF=0001234567 LEN=00003210 CRC=1A2B3C4D PID=3FA9C21B"]
    return "\n".join(L) + "\n"

worst = manifest(64, ["N" * 20] * 64)
typical = manifest(40, ["Llavero", "Soporte", "Tapa"])
print("manifiesto peor caso (64 objetos, 64 nombres de 20 B): %d B, linea mas larga %d B" % (len(worst), max(map(len, worst.split("\n")))))
print("manifiesto tipico (40 piezas, 3 nombres): %d B" % len(typical))
assert len(worst) < 5200 and len(typical) < 2100
