; ModuleID = 'writefile.bc'
source_filename = "writefile.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }

@.str = private unnamed_addr constant [6 x i8] c"%s/%s\00", align 1
@.str.1 = private unnamed_addr constant [4 x i8] c"123\00", align 1
@.str.2 = private unnamed_addr constant [3 x i8] c"wb\00", align 1
@.str.3 = private unnamed_addr constant [9 x i8] c"asdfsdf\0A\00", align 1

; Function Attrs: noinline nounwind optnone uwtable
define dso_local i32 @main(i32 %0, i8** %1) #0 !dbg !9 {
  %3 = alloca i32, align 4
  %4 = alloca i8**, align 8
  %5 = alloca i8*, align 8
  %6 = alloca [240 x i8], align 16
  %7 = alloca %struct._IO_FILE*, align 8
  %8 = alloca i32, align 4
  store i32 %0, i32* %3, align 4
  call void @llvm.dbg.declare(metadata i32* %3, metadata !16, metadata !DIExpression()), !dbg !17
  store i8** %1, i8*** %4, align 8
  call void @llvm.dbg.declare(metadata i8*** %4, metadata !18, metadata !DIExpression()), !dbg !19
  call void @llvm.dbg.declare(metadata i8** %5, metadata !20, metadata !DIExpression()), !dbg !21
  %9 = load i8**, i8*** %4, align 8, !dbg !22
  %10 = getelementptr inbounds i8*, i8** %9, i64 1, !dbg !22
  %11 = load i8*, i8** %10, align 8, !dbg !22
  store i8* %11, i8** %5, align 8, !dbg !21
  call void @llvm.dbg.declare(metadata [240 x i8]* %6, metadata !23, metadata !DIExpression()), !dbg !27
  %12 = getelementptr inbounds [240 x i8], [240 x i8]* %6, i64 0, i64 0, !dbg !28
  %13 = load i8*, i8** %5, align 8, !dbg !29
  %14 = call i32 (i8*, i64, i8*, ...) @snprintf(i8* %12, i64 240, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str, i64 0, i64 0), i8* %13, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.1, i64 0, i64 0)) #4, !dbg !30
  call void @llvm.dbg.declare(metadata %struct._IO_FILE** %7, metadata !31, metadata !DIExpression()), !dbg !92
  %15 = getelementptr inbounds [240 x i8], [240 x i8]* %6, i64 0, i64 0, !dbg !93
  %16 = call %struct._IO_FILE* @fopen(i8* %15, i8* getelementptr inbounds ([3 x i8], [3 x i8]* @.str.2, i64 0, i64 0)), !dbg !94
  store %struct._IO_FILE* %16, %struct._IO_FILE** %7, align 8, !dbg !92
  %17 = load %struct._IO_FILE*, %struct._IO_FILE** %7, align 8, !dbg !95
  %18 = call i64 @fwrite(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @.str.3, i64 0, i64 0), i64 1, i64 9, %struct._IO_FILE* %17), !dbg !96
  call void @llvm.dbg.declare(metadata i32* %8, metadata !97, metadata !DIExpression()), !dbg !98
  store i32 243, i32* %8, align 4, !dbg !98
  %19 = bitcast i32* %8 to i8*, !dbg !99
  %20 = load %struct._IO_FILE*, %struct._IO_FILE** %7, align 8, !dbg !100
  %21 = call i64 @fwrite(i8* %19, i64 4, i64 1, %struct._IO_FILE* %20), !dbg !101
  %22 = load %struct._IO_FILE*, %struct._IO_FILE** %7, align 8, !dbg !102
  %23 = call i32 @fclose(%struct._IO_FILE* %22), !dbg !103
  ret i32 0, !dbg !104
}

; Function Attrs: nofree nosync nounwind readnone speculatable willreturn
declare void @llvm.dbg.declare(metadata, metadata, metadata) #1

; Function Attrs: nounwind
declare dso_local i32 @snprintf(i8*, i64, i8*, ...) #2

declare dso_local %struct._IO_FILE* @fopen(i8*, i8*) #3

declare dso_local i64 @fwrite(i8*, i64, i64, %struct._IO_FILE*) #3

declare dso_local i32 @fclose(%struct._IO_FILE*) #3

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #1 = { nofree nosync nounwind readnone speculatable willreturn }
attributes #2 = { nounwind "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #3 = { "frame-pointer"="all" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }
attributes #4 = { nounwind }

!llvm.dbg.cu = !{!0}
!llvm.module.flags = !{!3, !4, !5, !6, !7}
!llvm.ident = !{!8}

!0 = distinct !DICompileUnit(language: DW_LANG_C99, file: !1, producer: "Ubuntu clang version 13.0.1-++20220120110844+75e33f71c2da-1~exp1~20220120230854.66", isOptimized: false, runtimeVersion: 0, emissionKind: FullDebug, enums: !2, splitDebugInlining: false, nameTableKind: None)
!1 = !DIFile(filename: "writefile.c", directory: "/home/cheong/carving_llvm/IR_example")
!2 = !{}
!3 = !{i32 7, !"Dwarf Version", i32 4}
!4 = !{i32 2, !"Debug Info Version", i32 3}
!5 = !{i32 1, !"wchar_size", i32 4}
!6 = !{i32 7, !"uwtable", i32 1}
!7 = !{i32 7, !"frame-pointer", i32 2}
!8 = !{!"Ubuntu clang version 13.0.1-++20220120110844+75e33f71c2da-1~exp1~20220120230854.66"}
!9 = distinct !DISubprogram(name: "main", scope: !1, file: !1, line: 5, type: !10, scopeLine: 5, flags: DIFlagPrototyped, spFlags: DISPFlagDefinition, unit: !0, retainedNodes: !2)
!10 = !DISubroutineType(types: !11)
!11 = !{!12, !12, !13}
!12 = !DIBasicType(name: "int", size: 32, encoding: DW_ATE_signed)
!13 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !14, size: 64)
!14 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !15, size: 64)
!15 = !DIBasicType(name: "char", size: 8, encoding: DW_ATE_signed_char)
!16 = !DILocalVariable(name: "argc", arg: 1, scope: !9, file: !1, line: 5, type: !12)
!17 = !DILocation(line: 5, column: 15, scope: !9)
!18 = !DILocalVariable(name: "argv", arg: 2, scope: !9, file: !1, line: 5, type: !13)
!19 = !DILocation(line: 5, column: 28, scope: !9)
!20 = !DILocalVariable(name: "outdir", scope: !9, file: !1, line: 6, type: !14)
!21 = !DILocation(line: 6, column: 10, scope: !9)
!22 = !DILocation(line: 6, column: 19, scope: !9)
!23 = !DILocalVariable(name: "buf", scope: !9, file: !1, line: 7, type: !24)
!24 = !DICompositeType(tag: DW_TAG_array_type, baseType: !15, size: 1920, elements: !25)
!25 = !{!26}
!26 = !DISubrange(count: 240)
!27 = !DILocation(line: 7, column: 8, scope: !9)
!28 = !DILocation(line: 8, column: 12, scope: !9)
!29 = !DILocation(line: 8, column: 31, scope: !9)
!30 = !DILocation(line: 8, column: 3, scope: !9)
!31 = !DILocalVariable(name: "f", scope: !9, file: !1, line: 9, type: !32)
!32 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !33, size: 64)
!33 = !DIDerivedType(tag: DW_TAG_typedef, name: "FILE", file: !34, line: 7, baseType: !35)
!34 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types/FILE.h", directory: "")
!35 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "_IO_FILE", file: !36, line: 245, size: 1728, elements: !37)
!36 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/libio.h", directory: "")
!37 = !{!38, !39, !40, !41, !42, !43, !44, !45, !46, !47, !48, !49, !50, !58, !59, !60, !61, !65, !67, !69, !73, !76, !78, !80, !81, !82, !83, !87, !88}
!38 = !DIDerivedType(tag: DW_TAG_member, name: "_flags", scope: !35, file: !36, line: 246, baseType: !12, size: 32)
!39 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_read_ptr", scope: !35, file: !36, line: 251, baseType: !14, size: 64, offset: 64)
!40 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_read_end", scope: !35, file: !36, line: 252, baseType: !14, size: 64, offset: 128)
!41 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_read_base", scope: !35, file: !36, line: 253, baseType: !14, size: 64, offset: 192)
!42 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_write_base", scope: !35, file: !36, line: 254, baseType: !14, size: 64, offset: 256)
!43 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_write_ptr", scope: !35, file: !36, line: 255, baseType: !14, size: 64, offset: 320)
!44 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_write_end", scope: !35, file: !36, line: 256, baseType: !14, size: 64, offset: 384)
!45 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_buf_base", scope: !35, file: !36, line: 257, baseType: !14, size: 64, offset: 448)
!46 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_buf_end", scope: !35, file: !36, line: 258, baseType: !14, size: 64, offset: 512)
!47 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_save_base", scope: !35, file: !36, line: 260, baseType: !14, size: 64, offset: 576)
!48 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_backup_base", scope: !35, file: !36, line: 261, baseType: !14, size: 64, offset: 640)
!49 = !DIDerivedType(tag: DW_TAG_member, name: "_IO_save_end", scope: !35, file: !36, line: 262, baseType: !14, size: 64, offset: 704)
!50 = !DIDerivedType(tag: DW_TAG_member, name: "_markers", scope: !35, file: !36, line: 264, baseType: !51, size: 64, offset: 768)
!51 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !52, size: 64)
!52 = distinct !DICompositeType(tag: DW_TAG_structure_type, name: "_IO_marker", file: !36, line: 160, size: 192, elements: !53)
!53 = !{!54, !55, !57}
!54 = !DIDerivedType(tag: DW_TAG_member, name: "_next", scope: !52, file: !36, line: 161, baseType: !51, size: 64)
!55 = !DIDerivedType(tag: DW_TAG_member, name: "_sbuf", scope: !52, file: !36, line: 162, baseType: !56, size: 64, offset: 64)
!56 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !35, size: 64)
!57 = !DIDerivedType(tag: DW_TAG_member, name: "_pos", scope: !52, file: !36, line: 166, baseType: !12, size: 32, offset: 128)
!58 = !DIDerivedType(tag: DW_TAG_member, name: "_chain", scope: !35, file: !36, line: 266, baseType: !56, size: 64, offset: 832)
!59 = !DIDerivedType(tag: DW_TAG_member, name: "_fileno", scope: !35, file: !36, line: 268, baseType: !12, size: 32, offset: 896)
!60 = !DIDerivedType(tag: DW_TAG_member, name: "_flags2", scope: !35, file: !36, line: 272, baseType: !12, size: 32, offset: 928)
!61 = !DIDerivedType(tag: DW_TAG_member, name: "_old_offset", scope: !35, file: !36, line: 274, baseType: !62, size: 64, offset: 960)
!62 = !DIDerivedType(tag: DW_TAG_typedef, name: "__off_t", file: !63, line: 140, baseType: !64)
!63 = !DIFile(filename: "/usr/include/x86_64-linux-gnu/bits/types.h", directory: "")
!64 = !DIBasicType(name: "long int", size: 64, encoding: DW_ATE_signed)
!65 = !DIDerivedType(tag: DW_TAG_member, name: "_cur_column", scope: !35, file: !36, line: 278, baseType: !66, size: 16, offset: 1024)
!66 = !DIBasicType(name: "unsigned short", size: 16, encoding: DW_ATE_unsigned)
!67 = !DIDerivedType(tag: DW_TAG_member, name: "_vtable_offset", scope: !35, file: !36, line: 279, baseType: !68, size: 8, offset: 1040)
!68 = !DIBasicType(name: "signed char", size: 8, encoding: DW_ATE_signed_char)
!69 = !DIDerivedType(tag: DW_TAG_member, name: "_shortbuf", scope: !35, file: !36, line: 280, baseType: !70, size: 8, offset: 1048)
!70 = !DICompositeType(tag: DW_TAG_array_type, baseType: !15, size: 8, elements: !71)
!71 = !{!72}
!72 = !DISubrange(count: 1)
!73 = !DIDerivedType(tag: DW_TAG_member, name: "_lock", scope: !35, file: !36, line: 284, baseType: !74, size: 64, offset: 1088)
!74 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: !75, size: 64)
!75 = !DIDerivedType(tag: DW_TAG_typedef, name: "_IO_lock_t", file: !36, line: 154, baseType: null)
!76 = !DIDerivedType(tag: DW_TAG_member, name: "_offset", scope: !35, file: !36, line: 293, baseType: !77, size: 64, offset: 1152)
!77 = !DIDerivedType(tag: DW_TAG_typedef, name: "__off64_t", file: !63, line: 141, baseType: !64)
!78 = !DIDerivedType(tag: DW_TAG_member, name: "__pad1", scope: !35, file: !36, line: 301, baseType: !79, size: 64, offset: 1216)
!79 = !DIDerivedType(tag: DW_TAG_pointer_type, baseType: null, size: 64)
!80 = !DIDerivedType(tag: DW_TAG_member, name: "__pad2", scope: !35, file: !36, line: 302, baseType: !79, size: 64, offset: 1280)
!81 = !DIDerivedType(tag: DW_TAG_member, name: "__pad3", scope: !35, file: !36, line: 303, baseType: !79, size: 64, offset: 1344)
!82 = !DIDerivedType(tag: DW_TAG_member, name: "__pad4", scope: !35, file: !36, line: 304, baseType: !79, size: 64, offset: 1408)
!83 = !DIDerivedType(tag: DW_TAG_member, name: "__pad5", scope: !35, file: !36, line: 306, baseType: !84, size: 64, offset: 1472)
!84 = !DIDerivedType(tag: DW_TAG_typedef, name: "size_t", file: !85, line: 46, baseType: !86)
!85 = !DIFile(filename: "/usr/lib/llvm-13/lib/clang/13.0.1/include/stddef.h", directory: "")
!86 = !DIBasicType(name: "long unsigned int", size: 64, encoding: DW_ATE_unsigned)
!87 = !DIDerivedType(tag: DW_TAG_member, name: "_mode", scope: !35, file: !36, line: 307, baseType: !12, size: 32, offset: 1536)
!88 = !DIDerivedType(tag: DW_TAG_member, name: "_unused2", scope: !35, file: !36, line: 309, baseType: !89, size: 160, offset: 1568)
!89 = !DICompositeType(tag: DW_TAG_array_type, baseType: !15, size: 160, elements: !90)
!90 = !{!91}
!91 = !DISubrange(count: 20)
!92 = !DILocation(line: 9, column: 10, scope: !9)
!93 = !DILocation(line: 9, column: 20, scope: !9)
!94 = !DILocation(line: 9, column: 14, scope: !9)
!95 = !DILocation(line: 12, column: 29, scope: !9)
!96 = !DILocation(line: 12, column: 3, scope: !9)
!97 = !DILocalVariable(name: "a", scope: !9, file: !1, line: 14, type: !12)
!98 = !DILocation(line: 14, column: 7, scope: !9)
!99 = !DILocation(line: 16, column: 10, scope: !9)
!100 = !DILocation(line: 16, column: 20, scope: !9)
!101 = !DILocation(line: 16, column: 3, scope: !9)
!102 = !DILocation(line: 18, column: 10, scope: !9)
!103 = !DILocation(line: 18, column: 3, scope: !9)
!104 = !DILocation(line: 20, column: 1, scope: !9)
