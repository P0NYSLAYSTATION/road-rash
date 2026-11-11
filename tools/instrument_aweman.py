"""Add a capture entry shim to a working x86 AWEMAN DLL, preserving its exports/code."""
from pathlib import Path
import hashlib
import struct

def instrument(source,output,helper_name='RRCAPTURE.DLL'):
    original=Path(source).read_bytes();data=bytearray(original)
    def u16(at):return struct.unpack_from('<H',data,at)[0]
    def u32(at):return struct.unpack_from('<I',data,at)[0]
    def put16(at,n):struct.pack_into('<H',data,at,n)
    def put32(at,n):struct.pack_into('<I',data,at,n)
    if data[:2]!=b'MZ':raise ValueError('Not a PE DLL')
    pe=u32(60);optional=pe+24;count=u16(pe+6);table=optional+u16(pe+20)
    if data[pe:pe+4]!=b'PE\0\0' or u16(pe+4)!=0x14c or u16(optional)!=0x10b:raise ValueError('Expected a 32-bit x86 DLL')
    sections=[]
    for i in range(count):
        at=table+i*40;sections.append((u32(at+12),u32(at+8),u32(at+20),u32(at+16)))
    if table+(count+1)*40>min(s[2] for s in sections if s[3]):raise ValueError('No section-header space; DLL left unchanged')
    def file_at(rva):
        for va,size,offset,raw in sections:
            if va<=rva<va+max(size,raw):return offset+rva-va
        raise ValueError('Unmapped PE RVA')
    align=lambda n,a:(n+a-1)//a*a
    section_alignment=u32(optional+32);file_alignment=u32(optional+36)
    va=align(max(v+max(s,raw) for v,s,off,raw in sections),section_alignment)
    raw_offset=align(len(data),file_alignment);old_entry=u32(optional+16)
    blob=bytearray(b'\0'*128)
    def append(value,alignment=1):
        while len(blob)%alignment:blob.append(0)
        at=len(blob);blob.extend(value);return va+at
    helper=append(helper_name.encode('ascii')+b'\0')
    dll_name=append(b'KERNEL32.dll\0')
    name=append(b'\0\0LoadLibraryA\0',2)
    lookup=append(struct.pack('<II',name,0),4)
    iat=append(struct.pack('<II',name,0),4)
    imports=bytearray();import_rva=u32(optional+104)
    if import_rva:
        at=file_at(import_rva)
        for _ in range(128):
            desc=bytes(data[at:at+20]);at+=20
            if len(desc)!=20:raise ValueError('Truncated imports')
            if not any(desc):break
            imports.extend(desc)
        else:raise ValueError('Import table too long')
    imports.extend(struct.pack('<IIIII',lookup,0,0,dll_name,iat));imports.extend(b'\0'*20)
    new_imports=append(imports,4)
    # DLL entry shim: call the working DLL's entry with unchanged arguments.
    # On successful PROCESS_ATTACH, load only the additive capture helper.
    code=bytearray.fromhex('55 89e5 53 56 57 ff7510 ff750c ff7508 8b4508 05')+struct.pack('<I',old_entry)+bytearray.fromhex('ffd0 85c0 0f84')
    failed_jump=len(code);code+=b'\0'*4
    code+=bytearray.fromhex('837d0c01 0f85');skip_jump=len(code);code+=b'\0'*4
    code+=bytearray.fromhex('8b5d08 8d83')+struct.pack('<I',helper)+bytearray.fromhex('50 ff93')+struct.pack('<I',iat)+bytearray.fromhex('85c0 0f95c0 0fb6c0')
    end=len(code);code+=bytearray.fromhex('5f5e5b 89ec 5d c20c00')
    for offset in [failed_jump,skip_jump]:struct.pack_into('<i',code,offset,end-(offset+4))
    if len(code)>128:raise ValueError('Entry shim overflow')
    blob[:len(code)]=code
    data.extend(b'\0'*(raw_offset-len(data)));data.extend(blob);data.extend(b'\0'*(align(len(blob),file_alignment)-len(blob)))
    header=table+count*40
    data[header:header+40]=struct.pack('<8sIIIIIIHHI',b'.rrcap\0\0',len(blob),va,align(len(blob),file_alignment),raw_offset,0,0,0,0,0xe0000060)
    put16(pe+6,count+1);put32(optional+16,va);put32(optional+56,align(va+len(blob),section_alignment));put32(optional+64,0)
    put32(optional+104,new_imports);put32(optional+108,len(imports))
    # Bound import metadata is obsolete after extending the import table.
    put32(optional+96+11*8,0);put32(optional+100+11*8,0)
    # Existing section contents, including original exports, stay byte-identical.
    for _,_,offset,size in sections:
        if data[offset:offset+size]!=original[offset:offset+size]:raise ValueError('Original DLL section changed')
    Path(output).write_bytes(data)
    return {'base_dll_sha256':hashlib.sha256(original).hexdigest(),'instrumented_dll_sha256':hashlib.sha256(data).hexdigest(),'original_entry_rva':old_entry,'capture_entry_rva':va,'helper':helper_name,'original_sections_preserved':True}
