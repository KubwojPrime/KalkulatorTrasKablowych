"""Generate the empty R2007 document and independently check exported DXF.

Development only: pip install ezdxf==1.4.4. No Python dependency in the app.
"""
import argparse
import io
from pathlib import Path
import ezdxf


def generate(path):
    ezdxf.options.write_fixed_meta_data_for_testing = True
    doc = ezdxf.new('R2007')
    doc.units = 4
    doc.header['$MEASUREMENT'] = 1
    doc.styles.get('Standard').dxf.font = 'arial.ttf'
    for name in ('TRASA', 'KABLE', 'PRZEPELNIENIE', 'OPISY', 'TABELA'):
        doc.layers.new(name, dxfattribs={'color': 1 if name == 'PRZEPELNIENIE' else 7})
    stream = io.StringIO()
    doc.write(stream)
    lines = stream.getvalue().splitlines()
    # Normalize only group-code whitespace for readable diffs and C++ tests.
    for i in range(0, len(lines), 2):
        lines[i] = str(int(lines[i]))
    data = '\n'.join(lines) + '\n'
    data = data.replace('0\nSECTION\n2\nENTITIES\n0\nENDSEC\n',
                        '0\nSECTION\n2\nENTITIES\n__KTK_ENTITIES__0\nENDSEC\n')
    assert data.count('__KTK_ENTITIES__') == 1
    assert doc.modelspace().block_record_handle == '17', 'Update C++ model owner'
    assert max(int(h, 16) for h in doc.entitydb) < 0x10000
    # Fixed metadata avoids meaningless timestamps/IDs changing on regeneration.
    for key in ('$TDCREATE', '$TDUPDATE', '$TDINDWG', '$TDUSRTIMER'):
        marker = '9\n' + key + '\n'
        if marker in data:
            a, b = data.split(marker, 1)
            parts = b.split('\n', 2)
            data = a + marker + parts[0] + '\n0.0\n' + parts[2]
    marker = '9\n$HANDSEED\n5\n'
    a, b = data.split(marker, 1)
    data = a + marker + '__KTK_HANDSEED__\n' + b.split('\n', 1)[1]
    Path(path).write_text(data, encoding='utf-8', newline='\n')


def validate(path):
    lines = Path(path).read_text(encoding='utf-8').splitlines()
    assert len(lines) % 2 == 0, 'Unpaired DXF group'
    tags = [(int(lines[i]), lines[i+1]) for i in range(0, len(lines), 2)]
    records = []
    for tag in tags:
        if tag[0] == 0:
            records.append([])
        if records:
            records[-1].append(tag)
    handles = [v for c, v in tags if c in (5, 105)]
    assert len(handles) == len(set(handles)), 'Duplicate handles (including HANDSEED)'
    handle_set = set(handles)
    expected = {'LTYPE': 'AcDbLinetypeTableRecord', 'LAYER': 'AcDbLayerTableRecord',
                'STYLE': 'AcDbTextStyleTableRecord', 'BLOCK_RECORD': 'AcDbBlockTableRecord'}
    for rec in records:
        kind = rec[0][1]
        if kind in expected:
            assert (100, 'AcDbSymbolTableRecord') in rec, f'{kind}: missing base subclass'
            assert (100, expected[kind]) in rec, f'{kind}: missing subclass'
        if kind in ('LINE', 'CIRCLE', 'TEXT'):
            owners = [v for c, v in rec if c == 330]
            assert len(owners) == 1 and owners[0] in handle_set, f'{kind}: missing owner'
            assert (100, 'AcDbEntity') in rec
    assert (2, 'BLOCK_RECORD') in tags and (2, 'BLOCKS') in tags
    assert (2, '*Model_Space') in tags and (2, '*Paper_Space') in tags
    # Ordinary read, deliberately not ezdxf.recover: no repair-mode acceptance.
    doc = ezdxf.readfile(path)
    audit = doc.audit()
    assert not audit.errors and not audit.fixes, (audit.errors, audit.fixes)
    model = doc.modelspace()
    assert len(model.query('CIRCLE')) == 3
    assert sorted(e.dxf.radius for e in model.query('CIRCLE')) == [5, 5, 10]
    assert all(e.dxf.owner == model.block_record_handle for e in model)
    texts = [e.dxf.text for e in model.query('TEXT')]
    assert any('YKYżo' in s for s in texts) and any('1.250*' in s for s in texts)
    assert doc.units == 4
    print(f'DXF structure, ownership, audit, cable geometry and Unicode passed: {path}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=('generate', 'validate'))
    parser.add_argument('path')
    args = parser.parse_args()
    (generate if args.action == 'generate' else validate)(args.path)
