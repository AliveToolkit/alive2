; https://bugs.llvm.org/show_bug.cgi?id=31243
target datalayout = "e-m:o-p:32:32-f64:32:64-f80:128-n8:16:32-S128"
target triple = "i386-apple-macosx10.11.0"

@data = global [4096 x i8] c"H\0D\BF\18\16,G\85?#!\84\87H\96\EE2\E7\13Y\0C<,\96+\09\9A\F5G1\B6s[\B7\D0\C2)\F0\A8\D9\B9%D|\B9EQU4\9C+\1B\07\09+\BB\B8\BD\B3\F5&\C0\C88\19H\10\0A\82\A9\0B4\9B\19\0C\E64\F6@\ACwE\BE\9D\9E\87Q \8D\08Rw\D9WH\F8o,\FE\C54&\0AdJ>J\B2f%\AFu\D4\09\BB\C5+\04)\1F\99\B3\CBRm\8EHH\9F\13\9F\F2\9E\A68\82\FE\FB4\E3[mH*!\BD8\98Ew\02\A4\19\EAer\04\FFf\DB1\EE\F4^\96v\04\16|r|\06\87\BEN\80\84\F2\16*\E0/\09\FE\5CH\E2.8A\D5o\D7\CBkeE4\80\99^\0C\FER\CF+]\1F\E6\08.\FB9\EC\15\DE\90\B1\D9j\E0\BA)G\16\9CX5!\83@\1A\042#Je\9D;\00\8DK&$\ACh8ZeC\99SD\FFP\A2\86\02\19\A0\D5\C9t\1F\04\16#\9B\B2\FE\AE\D1\A1\1B\85|<u#\0A\7F\1C@\91-\91\FBSw\A6\14\CC\83\8D\89\E5\DAx?@Z\CB-#\DB}y\9B?\EBO-\99\C6\C5\C9]\17\10UI\A3\90r\8F\0C,\A3DzCZr\BB\0C\AC\AE\A9\EC%\FE=\16\11\01R\07F-\D1\01\CDi#\98\E5\86\01\08\98\1A\FE\DCq\B89\FC\A2\22\06^\DB\B8>/\EA\8B\EF0\A0\9F\0A\E3\E6{\8B\84:4\B8\0C4=\DA\E1\98\E6\D1N\C5\C0\F0K\81\97\BD\9DZ\E0\8D?\ED\87\03P\BD\5C<\F4\A1\89\10\E5g\1D3\99\B3\CE\C8\FBNx\CC\CF\88\05:\F4\B2\90\1C\1F\FFNt\A9\0E\AE\02'0V\D3\A6X\C960\D7\C3\CF\E4L\EA\83\E6\94\DEk\07\89\88HWc\A3%h\D9xj\8Fz\5C\86e\0C\13\B9H\96I\F5\F8F\E0C\94\B9\97\AE\ACa\D4\E9\E2abz\D7\04\B6\8F)$\E8\FF\E9y\98{\C2`9V\1B\13^\A0l\12:E\1Ef\91\157:N\A4\F2\FDG\D6/\9C{xh-@\9B\1A\C1\8Az\C3yHc\F3:T\09\F7\FDp\A8\11\7F\F8V-\B3\D7C\03Z\8As\C7*\D0\D1^\B4\CF\C4Ju\DBU\EC\1F\C8[\FA\9D\92(\DC\F2\0D\A0\8DQ\04v\F9B\BC\B7\84]t7\C8B\B2\C8x\DF\EC\AF]h\A3:\01M_\00\A61\9FI[9\CElR8q\9F\E5\F6\AD\89X&c\94\9B\BEk\AE\E1\92!\CB_\0D\AB\0F\07\DCR1\A6\15\15\C4\0FY\FF \19uvf\F0\E8\9C\B2\A1=\D8\13\E8\93N\1Et*\B2rz\E3\A5\F2\B5Ty\EBP\B3\A1C\9C\A2\06\AF\1B\B7:\9B\8A\04U&~\EE\A0\EF\99\0D\B6\EEX\A2f\F9\AA\E7w!\22\E7$%\8FG\1Fv\B6\AB\95\94\87w-xN\E8\01\97\D3\CB\1Ai2\C7\0A \BA-\A2l\AB\D0\5C\93\9B\04t]\96 \FF\DA\FC\E3\AD\11\A9\ADM\03\B4\CF\F6\B9\EB\FC\E1\03\A1M(\16\A6\94\8F_\AFh\A5\D9\12d\08\B2\88\90\A4\83?\0A\10\D7\10\A4m\F7\BC\22SH\83\5C7\04C\90\C8\1C\DDm\87\B1\EE\0A\E3\90\1Ay\DAa\02BH\C3W\FC\C8\E3\DB\F0d\B1\FC\18\BF\1BE\15\FB\BDR&\DD\9C\14\A9\00\17M\C8^\1E*\CEN\A2\DF\F3m\9A\8C\9B3S\10\CD\AE[\A7YTX\03iC&\C5\81\EA\9F\0D\EB\B0\DF\DE\D8\15\A9\AE\9C\CE\86\90\8F\E2=\C5\B5\E2\B3\C5\1B\E0\A4\E6T\84\AD\0A\06j\BA\DA\F6\D7\DB\D8\0C\94T@\E7\1D\CB\D0\18\E8B\93*_x\15i\C2)e\ACr?/\B2\10\1A\B4\E6\A39\E1\98\D2\07\11\DDmy\B3\8Alv*,<\0C\C15\07\DA#\DC\C3\9B\843\0A\D6% \7F^\E9\B0\5C\94\10>\F2~mhX\EB\D3\F9\EC$\C7\00D\BC\B1j9\A8),U\C1\C3\22==0u\93\03N\08\BFc\9A<N\023\BB\CF\8E\94e(\FE\82\A2A\C2\C84Z\98\CDA\08m\D2\06\BD\F5\15\98\CD!3B\90\A5E\D4\EC\C3\07O\FC\F4|\C2\B9\1E \D1\0A\13\07\F3\83\DE\A3h\F7\C9xWm\01k\CC\C7Y\B6\B4\CDT\A3\F6\0E\16\13Ks`\FF7~\FB/\B13\F6\1D\E7\89\BC\C0\89\A3&\AB2u\1Dh.\18\91\81i\AB\10\FE_\9CR\FE\E7\B6\9C\BFE\86\0E\95\CE\7FL\A6\EF._\97\EF[V\BE\FD\C6/\DE\F4\D0\D9o\22\FE\16\1A\9A\C3\BF\F5\9B\AFY\90\11<T)\BE\09\EA^\11\D4J\11\1B\AA\D7\84o\A3I\C3e\03\FB\1E\88\90\FC}\90\83\F8V\D1o\CC9\EF%\EFK\B3\E3\1A:\A4]sL:\B2\E6U\8C\D8\E0FXsJA;\C8v9\B6\D4M\AD\FC\90\85\13\8BUi%\A3@a\18\DD\1F\DE\A9\BF-\04M Z\1B\E0GB\83w\8E\8B\12\C7\85\19\11l\C5q\AA\9F\A1\AC\93\A7\89f]\F26dRK\AB7\FC)\AF\91\22\BB\B5\0A\A9\8A\E7\C0m\A4\93.j\0A\C1X\7F0\99\8A\B6\AB\89\DC\E3\BB7\F7f#\03\9F\11+\F6\D6|\1F\E6kI2\03\BD\B3\C7\03\E6\8B\06(r\B4x(fb\AE\BDFm;\8EN\16\AFo\07?\C7L\AC\9B\89\F4\83\F6\D4\BB\AF\BA\970\B5g\F9QHa\5CR\89(\EC\15W\FF\A1K-(*\8Aj\ED\94\AFW\F6Fqvh\FC\8Cf\DE@\82N\02M\FCY\81\AE\CBo\92\FE\BC\07x,\80\98\AC6\86\14\B9?\15X\C2\C1\9E\03\D1\F5\1Chdn\854\D4@\BBG\D54\CA\E0\C4\13.M\82\1C\E8\06\AE_Hq|e+\14.\B0]Y\89\F8\93\CAb?\DE\CA\C2\B9,\97\C8W\1DI?\B3\CB\909\93\E3P\07g:\E9+z\E3\B1\0D\C2?\B1\94\C5\1D\0D\01^\BB\B4\DED\9A\E9S\BFy\A9\EF&\AF\D1n\CDqx\9C\AEG\F5\B1\FD\FB;!\90C\CB\89B\97\DB\8E\E3\93z!\02\04\F2q\18\F0M*\FFF\92v\DD\BD`L\1Fl\80\C14Z4\DB\F2\A8\FB\E8i\C9\A5\15\E9\05\9E\D2\B7u\F6C\F4u\E5\A0]\A0|\93\E7Pv\A2\B1\92\F0\08GrP4\10'\C2g!\E0T\CB-5o\B1P\F1\CCC;\E2\9E\E8\14\98\F7\99i\DF\F4\EF\EB$\DF{\10 \E9~^n\8E[\05L\FE\A6\FE\00\02}\A3\82=3g\F2P\8E\ECk\193\D1'{9G \D3\82\CC\A7\B83~\7F6b\BAB@\FF\17_\18y\B5\A4)OH?\01S\81aX\1F\E2cc\F2\F1Q\F9\AC\FE\EBV@\AA\B4M\0Df /\CF$\14%+\1E;D(\EEI  :K\18\8F\08\8E\05\AC\A7\ED\14\E3\C4\F1[\8C\F5\C4Fr\A3\FE\F1\EA\170\AE\C3\E1\FF\9Ez\CE\F2\F7\9A\D5\16\80\E5\93-I\D2R\F5\BF\AF\B4/*\DE\8Fu\CE\EB\F1\AA\A1\E8\F0J\E9\18\158$\9Dh\8A\5C\01\8Es\FF\80\E3=\93j\E1MD\DFc\9F\E0a\81\CB\E0)E\BER\EBw \95\F2\0Bg\8B\1B\AD6\F3x\12\B1\8Ez\C640\E7/\ABp\FF\E4\F9\19\11`\0DU\DA\12\91\B9\FE\DE\97\FC\AA'\02\C4\1F\C3\1D}\B9\0B!\DE\81Y\BE]PS@\94LE\00C\D0\D0\F2\BF8\BBF\9E\D9\9A\A3\F4PZH\E6\CB\8F\C1\14\98\FD\11\A4R(l\A8\FB{_<\CE\CEK\AB\07\F5p2\D2\03\0Cph\9F\B2\85\88\F8^\03\E8\D8{\F9u%\A7\AD4\B7\9Aj\D2\FB\E5O0\CD\F1L8,~3\8D-c8\A6\06{\91\D0A\81\A3N\A1\FEl\EE\D0\B3\C9\DEeS[\BE\8F\9C\11^[\FE#83\B0\84F\A9\E4\ADd\E3\FC\91F3!\C9\1Dy\D5v\A1\AE\00\EC\0E\5C+ F\DD9\81\11\85\CA\7FQ\BCP\8B\E7\A2\87\EB\15K\A3\D0\C1\C6P\E3\E21\1A\C3\14\FD\E5\B9g\EA\D5\FAY\E8?\A7\C0 \15\8E\89B\DA\AE\1A\E1\8549\11T\13\06u\C1R\E1\0A8\8A\0A\D8rRt\C1\CD\E2d\8B\8D\DA\B5\E0\A5\D83\BC\0A\04\F6y\AA\B8\F6\89&\9Ds\C8\FB\CBG\82\BEm\FCV\C3N\E2\01\7F\13\E18\EC\81P\82SR\F6i\B6\11\FD@ir<<,\8A/\E6\96\D2>\A0\F7\96\92\B1\AB\90\07. i\87\BD\E7\1A5.\A3\DB\E4\CE$\04/\B1WZ\94e\C7W,\E7\C4rV\DBn\E37>\D2\DA\0F\F3\95\BBE\14\BEYp\A7\0BOK^yif\5C\90\80D\01\E4\A4\E1G\D4\EE\1C\F1\86\E2\E2\CAI\AA\A5\D9\08l\8DM\0B\F4*\10Q\EE\C9\98\22sK\11\A6\C8\A1\D1\F8\F5_\EC\06\A2\DA\B7\D5e\8A\C4w\F1{\8Fv\00\10\D7\9E\A9\9B\AE\C9\E6\B1\CE\A7r\AA\C82`)z\9Ai\E5\CCw\9B\9F\DC\06\91c\C1C\DB\97$\F1\0A\DE%9\F7BTk&\D2\B5*\8D\11\9C.\DA&*_+`_\EE\22\15\F1\A0\16\BF\CC\04[ZV3N\B6\F4\F7O\10p\D0~\B4o\B6\0B6\03\9AKx\D4\FDm\D0~=\01Y\98a\AD0\E3\A3.\F0\80\F8\1B\0D\00\C7\8A\A7\18\F0\AFR\D67k\B1W\91\E2\E3\C8o\DC\175\DD\97'Of\FF4\BC\AD\AE\C5\9F\93P|\D3\D6\E2'\1A\5C^\D3\B6%5\0C3\92\D1\17\B9\1Cz\BD\1C\E1J\C1\AC\12B\C8\DA\C0\81\BD\CF\B6+\8F\08\8C\0A\F2y\B8\BA\14\19\F7a\B4\C5\99\BB\16_\8A\1F\FC/\8F\BE\E8\05\BB\CAP6\1B\F0B\17\C3\16\06\DBtk\86e\99\BE2\B5\BA\EA\97u7M`\8A\E7\DD\A7\AC_\8A\F0\0Cx\D3WT\AF\85g\F6:E\CF\ADB\8E\BE\C1\B3\D8\1E\0D,\85\BF\02\FEd9O\BD\B9Z\19\E0V\B3o\EA\12\F9\D7\A0\B8\C4\D1\9FD\11<1\5C\0ARr\A8A\D4\D3!\8C\80-\C9j\A1\C3\EEq\98>a{8\ECx\9A>,u\9A\B12q\16\5C\06?\22A\BB\C4h\E4\8A\1D\13\17\12\BB\05Sz,D\98m\CE\8F\11\EA\96\86\BB\CCb\DE,O\AD\04\CC\CF\85y\D2\AD\EA\85\8DZM2\1E\C2DU\B71\1C\D7\F1jM\D9\B22\B4+\0A\0C\C2;Ii\DA\E1g\09\BBag\A5<\EC\D6\D6\E5\19\A8\C6\ECm`\19\90?U\FF5:\F9\D7{9\1F\8C\D9\8DT\F9\84\0CK\E1\E6%b\D8\BF\C1\13\8D\EC\F6;\CBe\F4\B9\D3\A1\E3\1F7\CB\E2\E5;H7\9A3\DAkA\D0r\CF\E7\85C\82}\DAT\C6\0BP:s\11\A7Q\FF<6nQFm\9BR\9A\02\E4\ED\CC\E6f}\B2\CDE\AC\DC\EC\C9=\05\AF7\A5d\8F\C2\B1\8C\D6\00\89\C8\D8\8C\ABy\B1?v4m0\DF\12:/\15\A7\0BM\AD\11\F4\E1Rw\89\FDB\C7\AE\82\1BA\E1\E4a\9F\DAX\05P,\9Ap\D50\DC\C7*\DF\BE{\B2\C2\84\BF\FF1ne&\96^\A0\A8 \95D-\88\D0'sG\87\94\C3\1A\ECD\81V$\91N\C9\E8\B9\BC\1AN|d.f[@\A3**(\F1\D4\BF\D5\AD}\CC\0B\F4\12\98?`\8D\D5\9CR\03\F5\9B\C2\D7\DC\C7\B2\9B\A1G\8C2 \98\96\84\80\FD\B2Y)wZ\A0\E3\D3,\13\D3\AB\D8\0B\C4\B1W\96t7\9CI\960\1A\F91\9F\F9\E6\94\9A\A9\86W\CB\89\98\7F\88\91\8E\18Cm\F7JX\A2\93\C56\C5\1E\1A\81x\05\0A\90r'\93#@L\B5=\1D\94)\8D\11\92\D3\92\89\A2\D4\9FZB\9C\1A\C7\95e@\AB\F05\FFc~\BF\02\CE!\E9\0C\0B?\C8k\5C\EF\16\C8>:\83:\D6\1C;\C5\DA\E2\0B\A6@\F5\93\0AE\A9n\FD\09\11\EC2r\00\9ER\B6A\8A\81\C0K?G\8Fv\7FMm\D3\0C\81\BE\D9\BF\A5\17#\D7\FFQ\EA-8\85@\95F\DD\89\00\D8\87\1F\05\B2D`g\EB\D8\DDR\17\A7'C\DA\BDQ\9E\A3n\CF?^f\BE\E7\C7\13;\17}J\B6\B9\A4D\C2m\9Bb/K\D7\AE\93\9B&\17\0D\D6\F9\8Fb6=\A0\C15ow\E4\19A\E0I\FFF6-?j\DE\B3\1F\FB\91\ECWn\B9\85\0E[n\B2#5\CE\87\D4:P>\02\17\5C\87\FE \AEp\15|\FE\9A\C4\DB\BB\BB\C5\8D+\00\AD\C5(1\CE\8C\A2EB\D99\E4\B9bI\F9`\C1\B2\89\C2\BB\A4\DB\89,7\9BD\15\0B\FAq\92sn,\B7\15c@9\CC~\A9F\FB\87\F9\82\A1\C4v\04\9E^\B8<B\AE`\F2n\AD\A7\CF\CE\9B49(\0C\FD\1DA\B04\A2S=\DC\19\A8&\0150\D4\E4`\A0\B8J\1D\9A\97\B7\8F\B2\BAoAN\16y\C7\E1\AA-\02Ff\17\A4\0EZp\EE\C3D>el\EB\CCB\A2i\DBl\228\BF\03\03\16\A2\89\C7\A8K\DEd`\C9\00\AC\A0Pd\11\0E\A8\1DR\92\E2m4a\10\82\C6\F9\BD\AD\F0\8C:\B9\CF\A9\8F\DE\178\A8j\C3'\EF>\7F94\A4\FCg\FC\0D\BB\EF\E7\D5\FF\BD`\CEa\079\FFR3\19\AA\ED\DD\1A\A6\81U\B0\FD\14\14p$\AD<!\06\B5\D3\C5'|\A9\FC\22{\09\F7\91\CE:3B(\84i\D5w`\D5F\AF\86gRL:T\C3\1D\D6\05\8E\19v*\F1\94p\9C\B4\0D\AC}i\CD\AD@\13\E7\0B O\8D\BB\8DD\A4\18\D7;\9B\90\1A\00\14\A8l\A4J\93\F1\BF\E1\FF\1D}\FD\FAje'T\EF.\F5\CFu\18\FD\8A\FF\E5\B1\FEN~GT\C8\B6}\04\8A34E\07e\A1\89Ec\A6\A9o\91\12\F5\0E\F4\82P+\B2\CD\80\A8d \F5\C2\1E\85\BC$\EB\D6\C9\1B?\83fva\DB\B5%j\CD\A4\C7\1A\AC\05n\80\7FI\EEJ\B5u;Q\A5}\92\E7\DA\C4\0C\C5\11\A5\DCN\1B\BD\AC1\AC\1E\E3?UP\A1\F5\C5|\F1\C0\B4',\AC\D3\D1r\EDf;p\13\01SN\D4$\01\F6\D3\8D`\D5uH\E7\D6'\F2fM\03+\EF!)\A7\AE\F5\C7\F1S\AF\C3\1F\D9-KT\A3\01\C8\F9>\C0\94\16sE\B0\B6\DCZi\96 N\9C\12\CC\B2$z\FB\A1\05\DBy\FB\86\03\C0<}s\BE$\1F\F9\9D`_d\E6\0E\E4\93\A1\02\B7\8E\EE+f\C9_pv\B3\C7\9D\16\A5\8C\10\F1\5Cb\5C\F7\8E\D8\17#P\ED+geN,\DB\06\06\D3E\A8\E2\92\1A&\C9\00\D8}\AD\B5\D6\93\D5L)l\F3\8E\07\89ze\F8\F9\F94\A4\A2Uq\0Ft\8A\11\B3@-\C7\F3y\94\89L\B7\CC\84_Z\AE{6\0E\22.na\90\DD\F3\A6\0D\ED\09@\E7\FF\EB\8B'\BA\DFl?nE\1F\AF\BF\8FI\EAo\1D\22\DDJ\0Brq\E5e\B13\88O\9F\A3\A6T8\1D\C6\93_\A2k\B5\FF\11l\89\A8\B35m\E2\A0\E8V\A919(\D9hztUM~o(D\DA\C39\86\A4k\B4\8A\5C\CFt\B8\18\1F/@q\15\8F\F1X\CA\AAXwq\95\C8Tu\A6\0A\C3w\1C\9F\ED:\B9A\88L\AD\0C\D2u1\1A8\B09\FFg\FD\11\080\F8\0C_\F1d\CD\D5#*\F7\E4\EB\07\94[F\D9n\09\8Eh\D7\1Fz\CA\B81\AF\EC-\DB\16W,\DC\EDe\87\80\1C\E8A\1F.\FD\A7\F4*\B7\C4\C8\F2C\F8\C0\0D\01\EB\EA\EF\0B\A6\F6\D0\D5U\07:`\BA\C9N4\94Y\AC\D2\9B", align 1

; Function Attrs: nounwind ssp
define i32 @src(i8 zeroext %v1, i8 zeroext %v2, i8 zeroext %v3, i8 zeroext %v4, i8* %ptr) #0 {
entry:
  %xor1 = xor i8 0, %v1
  %xor2 = xor i8 0, %v2
  %xor3 = xor i8 0, %v3
  %xor4 = xor i8 0, %v4

  %add1 = add i8 %xor1, 30
  %add2 = add i8 %xor2, 29
  %add3 = add i8 %xor3, 48
  %add4 = add i8 %xor4, 67

  %shl1 = shl i8 %xor1, 1
  %shl2 = shl i8 %xor2, 1
  %shl3 = shl i8 %xor3, 1
  %shl4 = shl i8 %xor4, 1

  %and1 = and i8 %shl1, 60
  %and2 = and i8 %shl2, 58
  %and3 = and i8 %shl3, 96
  %and4 = and i8 %shl4, -122

  %sub1 = sub i8 %add1, %and1
  %sub2 = sub i8 %add2, %and2
  %sub3 = sub i8 %add3, %and3
  %sub4 = sub i8 %add4, %and4

  %tmp1 = xor i8 %sub1, 30
  %tmp2 = xor i8 %sub2, 29
  %tmp3 = xor i8 %sub3, 48
  %tmp4 = xor i8 %sub4, 67

  %xor11 = zext i8 %tmp1 to i32
  %xor12 = zext i8 %tmp2 to i32
  %xor13 = zext i8 %tmp3 to i32
  %xor14 = zext i8 %tmp4 to i32

  %arrayidx1 = getelementptr inbounds i8, i8* %ptr, i32 %xor11
  %arrayidx2 = getelementptr inbounds i8, i8* %ptr, i32 %xor12
  %arrayidx3 = getelementptr inbounds i8, i8* %ptr, i32 %xor13
  %arrayidx4 = getelementptr inbounds i8, i8* %ptr, i32 %xor14

  %load1 = load i8, i8* %arrayidx1, align 1
  %load2 = load i8, i8* %arrayidx2, align 1
  %load3 = load i8, i8* %arrayidx3, align 1
  %load4 = load i8, i8* %arrayidx4, align 1

  %conv1 = zext i8 %load1 to i32
  %conv2 = zext i8 %load2 to i32
  %conv3 = zext i8 %load3 to i32
  %conv4 = zext i8 %load4 to i32

  %sum1 = add nuw nsw i32 %conv1, %conv2
  %sum2 = add nuw nsw i32 %sum1, %conv3
  %sum3 = add nuw nsw i32 %sum2, %conv4
  ret i32 %sum3
}

define i32 @tgt(i8 zeroext %v1, i8 zeroext %v2, i8 zeroext %v3, i8 zeroext %v4, i8* %ptr) #0 {
entry:
  %0 = insertelement <4 x i8> undef, i8 %v1, i32 0
  %1 = insertelement <4 x i8> %0, i8 %v2, i32 1
  %2 = insertelement <4 x i8> %1, i8 %v3, i32 2
  %3 = insertelement <4 x i8> %2, i8 %v4, i32 3
  %4 = xor <4 x i8> zeroinitializer, %3
  %5 = add <4 x i8> <i8 30, i8 29, i8 48, i8 67>, %4
  %6 = shl <4 x i8> %4, <i8 1, i8 1, i8 1, i8 1>
  %7 = and <4 x i8> <i8 60, i8 58, i8 96, i8 -122>, %6
  %8 = sub <4 x i8> %5, %7
  %9 = xor <4 x i8> <i8 30, i8 29, i8 48, i8 67>, %8
  %10 = zext <4 x i8> %9 to <4 x i32>
  %11 = trunc <4 x i32> %10 to <4 x i8>
  %12 = extractelement <4 x i8> %11, i32 0
  %13 = sext i8 %12 to i32
  %arrayidx1 = getelementptr inbounds i8, i8* %ptr, i32 %13
  %14 = extractelement <4 x i8> %11, i32 1
  %15 = sext i8 %14 to i32
  %arrayidx2 = getelementptr inbounds i8, i8* %ptr, i32 %15
  %16 = extractelement <4 x i8> %11, i32 2
  %17 = sext i8 %16 to i32
  %arrayidx3 = getelementptr inbounds i8, i8* %ptr, i32 %17
  %18 = extractelement <4 x i8> %11, i32 3
  %19 = sext i8 %18 to i32
  %arrayidx4 = getelementptr inbounds i8, i8* %ptr, i32 %19
  %load1 = load i8, i8* %arrayidx1, align 1
  %load2 = load i8, i8* %arrayidx2, align 1
  %load3 = load i8, i8* %arrayidx3, align 1
  %load4 = load i8, i8* %arrayidx4, align 1
  %conv1 = zext i8 %load1 to i32
  %conv2 = zext i8 %load2 to i32
  %conv3 = zext i8 %load3 to i32
  %conv4 = zext i8 %load4 to i32
  %sum1 = add nuw nsw i32 %conv1, %conv2
  %sum2 = add nuw nsw i32 %sum1, %conv3
  %sum3 = add nuw nsw i32 %sum2, %conv4
  ret i32 %sum3
}


attributes #0 = { nounwind ssp "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="true" "no-frame-pointer-elim-non-leaf" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="yonah" "target-features"="+cx16,+fxsr,+mmx,+sse,+sse2,+sse3,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"PIC Level", i32 2}
!1 = !{!"clang version 4.0.0 (trunk 283264)"}

; ERROR: Source is more defined than target
