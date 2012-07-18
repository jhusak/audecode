{ h
Audio to disk image converter
Author: Jakub Husak
Date: 07-2012

This is complete freeware, do with the code what you want.
I am not responsible for any activity of this program,
however I have tested it and it looks like it does the job.

TODO:
test on various images (SD,DD)
"need logreset"
Change name of the app
instruction manual
}
unit main;

{$mode objfpc}{$H+}
interface

uses
  Classes, SysUtils, FileUtil, Forms, Controls, Graphics, Dialogs, StdCtrls, sdl, LCLType ;



const MAX_SECT_NUM = 2880;
type
  TWaveState = record
    resol: Integer;
    levels: Integer; // 256 or 65536
    next: Integer;
    endian: Integer;
    signed: Integer;
    start_ptr: PUInt8;
    curr_ptr: PUInt8;
    bytelength: LongWord;
    edge_threshold:integer;

    prev_val:integer;
    curr_val:integer;

  end;

  SStatus = (ssEnd,ssEmpty,ssGood,ssBad);
  { TMainForm }

  TMainForm = class(TForm)
    Btn_ClearLog: TButton;
    Btn_ResetAll: TButton;
    Btn_ShowDisk2AudBas: TButton;
    Btn_OpenWav: TButton;
    Btn_SaveXfd: TButton;
    Btn_SaveAtr: TButton;
    InfoTitle: TLabel;
    InfoTitle1: TLabel;
    InfoTitle2: TLabel;
    InfoTitle3: TLabel;
    InfoTitle4: TLabel;
    LogWindow: TMemo;
    LogWindowLabel: TLabel;
    Memo1: TMemo;
    OpenWavDialog: TOpenDialog;
        SaveXfdDialog: TSaveDialog;


    procedure Btn_ClearLogClick(Sender: TObject);
    procedure Btn_OpenWavClick(Sender: TObject);
    procedure Btn_ResetAllClick(Sender: TObject);
    procedure Btn_SaveXfdClick(Sender: TObject);
    procedure Btn_SaveAtrClick(Sender: TObject);
    procedure Btn_ShowDisk2AudBasClick(Sender: TObject);
    procedure FormCreate(Sender: TObject);
  private
    { private declarations }
    analyze_buffer: array[0..65535] of integer;
    chksum: UInt8;
    MaxSector: integer;
    xfd_sector_data:array [0..256*MAX_SECT_NUM] of Uint8;
    xfd_sector_status: array[0..6000] of SStatus;
    xfd_sector_length: integer;
    wave_state: TWaveState;
    fname: array[0..255] of char;



    procedure AnalyzeLevels;
    function CalculateVolume: integer;

    function DecodeSector: SStatus;
    function GetImpulseLength: integer;
    function GetNextDiff: integer;
    function GetNextSample: integer;
    procedure OutputLengths;
    procedure SaveDiskImage(lfname:string;header: boolean);
  public
    { public declarations }
  end;

var
  MainForm: TMainForm;

implementation

{$R *.lfm}

{ TMainForm }

procedure TMainForm.Btn_OpenWavClick(Sender: TObject);
var
    wav_spec: TSDL_AudioSpec;
  wav_bytelength: UInt32;
  wav_buffer: PUInt8;

  pfname: Pchar;
  i:integer;
  z:integer;
  sector_status: SStatus;
  GoodSectors, BadSectors, MissingSectors: integer;
  tmp_thr: LongInt;
  tmp_ws: TWaveState;
  vol,vol2:integer;
  tmp_ptr: PUInt8;
  init_threshold:boolean;
  tbadsects: LongInt;
  tail: Integer;
begin
  if not OpenWavDialog.Execute then exit;

  Btn_SaveXfd.Enabled:=false;
  Btn_SaveAtr.Enabled:=false;

  fname:=OpenWavDialog.FileName;
  pfname:=@fname;
  try

  SDL_LoadWAV(pfname, @wav_spec, @wav_buffer, @wav_bytelength);

  LogWindow.Append('File bytelength: '+inttostr(wav_bytelength));
  LogWindow.Append('File chn   : '+inttostr(wav_spec.channels));
  LogWindow.Append('File smpl  : '+inttostr(wav_spec.samples));
  LogWindow.Append('File freq  : '+inttostr(wav_spec.freq));
  LogWindow.Append('File format: '+inttohex(wav_spec.format,4));


  with wave_state do
  begin
  resol:=(wav_spec.format and $1f) div 8; // 1 or 2 bytes
  levels:=256;
  if resol=2 then levels*=256;
  next:=wav_spec.channels * resol; // how many bytes to move to next sample
  endian:=(wav_spec.format and $1000) div $1000;           // 0 or 1
  signed:=(wav_spec.format and $8000) div $8000;             // 0 or 1
  curr_ptr:=wav_buffer;
  start_ptr:=wav_buffer;
  bytelength:=wav_bytelength;
  prev_val:=0;
  curr_val:=0;
  end;

  LogWindow.Append('length byt : '+inttostr(wav_bytelength));
  LogWindow.Append('channels   : '+inttostr(wav_spec.channels));
  LogWindow.Append('frequency  : '+inttostr(wav_spec.freq));
  LogWindow.Append('resolution : '+inttostr(wave_state.resol * 8));
  LogWindow.Append('signed     : '+booltostr(wave_state.signed>0,'yes','no'));
  LogWindow.Append('big endian : '+booltostr(wave_state.endian>0,'yes','no'));

  wave_state.curr_ptr:=wave_state.start_ptr;
  vol:=CalculateVolume;
  vol2:=0;
  if wav_spec.channels = 2 then begin
     wave_state.curr_ptr:=wave_state.start_ptr + wave_state.next div 2;
     vol2:=CalculateVolume;
     if vol2>vol then begin
       vol:=vol2;
       wave_state.curr_ptr:=wave_state.start_ptr + wave_state.next div 2;
       LogWindow.Append('Using louder channel 2');
     end
     else
     begin
          wave_state.curr_ptr:=wave_state.start_ptr;
          LogWindow.Append('Using louder channel 1');
     end;
  end;
  Application.ProcessMessages;



  tmp_ptr:=wave_state.curr_ptr;
  AnalyzeLevels;
  wave_state.curr_ptr:=tmp_ptr;

  init_threshold:=true;
  tail:=1;
  BadSectors:=0;
  repeat
        tmp_ws:=wave_state;
        GoodSectors:=0;
        tbadsects:=BadSectors;

        BadSectors:=0;
        MissingSectors:=0;
        LogWindow.Append('Trying threshold...'+IntToStr(wave_state.edge_threshold));
        Application.ProcessMessages;
        repeat
          sector_status:=DecodeSector;

        until sector_status=ssEnd;
      //for sector_status in xfd_sector_status do



      for i:=1 to MaxSector do
      begin
           if xfd_sector_status[i]=ssGood then inc(GoodSectors);
           if xfd_sector_status[i]=ssBad then inc(BadSectors);
           if xfd_sector_status[i]=ssEmpty then inc(MissingSectors);
      end;

      LogWindow.Append('Good Sectors    : '+IntToStr(GoodSectors));
      LogWindow.Append('Bad Sectors     : '+IntToStr(BadSectors));
      LogWindow.Append('Missing Sectors : '+IntToStr(MissingSectors));
      LogWindow.Append('Max Sector num  : '+IntToStr(MaxSector));
      Application.ProcessMessages;
      wave_state:=tmp_ws;

      if init_threshold then
        with wave_state do
          begin
            edge_threshold:=4;
            if resol=2 then edge_threshold:=8;
            //curr_ptr := tmp_ptr;
            init_threshold:=false;
          end;
      if tbadsects<>BadSectors then
          begin

          if tail=0 then
          wave_state.edge_threshold-=wave_state.edge_threshold div 4;
          tail:=16;

          end
      else
          if tail >0 then dec(tail);

      if tail=0 then
          wave_state.edge_threshold+=wave_state.edge_threshold div 2
      else
          wave_state.edge_threshold+=wave_state.edge_threshold div 32+1;


  until ((BadSectors=0) and (GoodSectors>0) and (MissingSectors=0)) or (wave_state.edge_threshold > MulDiv(wave_state.levels,1,2));
  if (BadSectors>0) or (MissingSectors>0) then
    begin
         LogWindow.Append('WARNING. The image is incomplete!');
    end;

  LogWindow.Append('Finished');
  Application.ProcessMessages;

  SDL_FreeWAV(wav_buffer);


  Btn_SaveXfd.Enabled:=true;
  Btn_SaveAtr.Enabled:=true;
  Btn_SaveAtr.SetFocus;
  except
    LogWindow.Append('An error occured during reading file '+OpenWavDialog.FileName);
  end;


  {
  AUDIO_U8 = $0008; // Unsigned 8-bit samples
  AUDIO_S8 = $8008; // Signed 8-bit samples
  AUDIO_U16LSB = $0010; // Unsigned 16-bit samples
  AUDIO_S16LSB = $8010; // Signed 16-bit samples
  AUDIO_U16MSB = $1010; // As above, but big-endian byte order
  AUDIO_S16MSB = $9010; // As above, but big-endian byte order
  AUDIO_U16 = AUDIO_U16LSB;
  AUDIO_S16 = AUDIO_S16LSB;
  }


end;

procedure TMainForm.Btn_ResetAllClick(Sender: TObject);
var i:integer;
begin
  //
  Btn_SaveXfd.Enabled:=false;
  Btn_SaveAtr.Enabled:=false;
  // init values
    for i:=0 to length(xfd_sector_status)-1 do
      xfd_sector_status[i]:=ssEmpty;
    MaxSector:=0;
    LogWindow.Clear;


end;

procedure TMainForm.SaveDiskImage(lfname:string;header:boolean);
var
    corr: integer;
    res:integer;
    headerlen: integer;
    f: file of UInt8;
    i:integer;
begin

     AssignFile(f,lfname);
       rewrite(f);

       headerlen:=0;
       corr:=0;
       if xfd_sector_length=256 then corr:=3*128;

       if header then
         begin
            for i:=Length(xfd_sector_data)-17 downto 0 do
                xfd_sector_data[i+16]:=xfd_sector_data[i];

            headerlen:=16;
            for i:=0 to 15 do xfd_sector_data[i]:=0;
            xfd_sector_data[0]:=$96;
            xfd_sector_data[1]:=$02;
            xfd_sector_data[2]:=((MaxSector*xfd_sector_length - corr) div 16) and $ff;
            xfd_sector_data[3]:=((MaxSector*xfd_sector_length - corr) div 16) div $100;
            xfd_sector_data[4]:=xfd_sector_length and $ff;
            xfd_sector_data[5]:=xfd_sector_length div $100;
         end;



       BlockWrite(f,xfd_sector_data, MaxSector*xfd_sector_length - corr + headerlen,res);
       CloseFile(f);
       LogWindow.Clear;

       if MaxSector*xfd_sector_length - corr + headerlen = res then
         LogWindow.Append('File saved.')
       else
         LogWindow.Append('File save failed. (saved '+inttostr(res)+' bytes)');

       Application.ProcessMessages;

       sleep(2000);

       MaxSector:=0;
       Btn_ResetAllClick(nil);


end;

procedure TMainForm.Btn_SaveAtrClick(Sender: TObject);
var
i:integer;
begin

  i:=LastDelimiter('.',fname);
  SaveXfdDialog.FileName:=  Copy(fname,1,i)+'atr';
  if SaveXfdDialog.Execute then
    begin
       SaveDiskImage(SaveXfdDialog.FileName,true);
    end;
end;
procedure TMainForm.Btn_SaveXfdClick(Sender: TObject);
var
i:integer;
begin
    i:=LastDelimiter('.',fname);
    SaveXfdDialog.FileName:=  Copy(fname,1,i)+'xfd';

  if SaveXfdDialog.Execute then
    begin
       SaveDiskImage(SaveXfdDialog.FileName,false);
    end;
end;

procedure TMainForm.Btn_ShowDisk2AudBasClick(Sender: TObject);
var s:string;
const disk2snd : array [0..33] of string= ('0 . DISK2SND COPIER',
'1 . WITHOUT ANY CABLES.',
'2 . BY JAKUB HUSAK, DATE:07.2012',
'4 . JUST RETYPE THIS PROGGY,',
'5 . PUT THE DISK INTO DRIVE 1',
'6 . YOU WANT TO CONVERT TO XFD,',
'7 . ENTER NUMBERS AND RECORD',
'8 . OUTGOING NOISE ON PC, THEN',
'9 . SAVE AS WAV AND FEED THE PC APP',
'10 SUM=13218 : PLEN=145',
'11 POKE 65,0:  DIM A$(4), B$(512)',
'12 FOR I=1 TO PLEN*2 STEP 4: READ A$: B$(I,I+4)=A$:N.I',
'13 FOR I=1 TO PLEN*2 STEP 2: VAL=(ASC(B$(I))-65)*16+ASC(B$(I+1))-65:POKE 1536+(I-1)/2,VAL:SUM=SUM-VAL:N.I',
'14 IF SUM<>0 THEN ? "DATA ERROR": END',
'15 ? "START SEC?": I. SSEC',
'16 ? "END SEC?": I. ESEC',
'17 ? "SECT LEN [1]28/[2]56?":I. SLEN',
'18 FOR I=SSEC TO ESEC: ? "READING SECTOR: ";I,"ST: ";:? USR(1543,I,I,128+(I>3)*(SLEN=2)*128): N.I',
'19 ? "OPERATION COMPLETED." : END',
'20 D. DBAB,FCEA,AIAE,BOKC,AGLN',
'21 D. AAAG,JNAA,ADGI,JNAI,ADMK',
'22 D. BAPD,CAFJ,OEIF,NFIE,NEBA',
'23 D. ABGA,HICA,FIAG,KJFH,IFDB',
'24 D. CAGI,AGKC,AJIN,AKNE,CAGC',
'25 D. AGNA,PICA,GFAG,KAAE,KJAI',
'26 D. IFHO,KJAD,IFHP,CAHG,AGOG',
'27 D. HPKM,AIAD,CAHG,AGKF,DBKA',
'28 D. AACA,HJAG,KAEA,KJCC,IMAO',
'29 D. NEIN,AANE,FIGA,JAAD,INAK',
'30 D. NEKF,DFCM,KJBP,EJAP,IFDF',
'31 D. INAK,NEIN,ABNC,MKGA,IILB',
'32 D. HOIF,DCBI,GFDB,CKGJ,AAIF',
'33 D. DBKC,AHAG,DCCA,GAAG,BAPJ',
'34 D. MAAA,NAOG,GA'
);
begin

  for s in disk2snd do LogWindow.Append(s);

end;


procedure TMainForm.Btn_ClearLogClick(Sender: TObject);
begin
  LogWindow.Clear;
end;

procedure TMainForm.FormCreate(Sender: TObject);
var i: integer;
begin
  Btn_ResetAllClick(nil);

end;

function TMainForm.GetNextSample: integer;
begin
  result:=-$ffff;
  with wave_state do begin
  if (curr_ptr - start_ptr) >= bytelength-3 then exit;

  prev_val:=curr_val;
  if resol = 1 then
    begin
         result:=curr_ptr^;
         if signed>0 then
            if result>=128 then
               result-=256;
    end
  else
    begin
      if endian =1 then
         result:=256 * (curr_ptr^) + ((curr_ptr +1)^)
      else
          result:=256 * ((curr_ptr+1)^) + (curr_ptr^);
      if signed>0 then
         if result>=32768 then
            result-=65536;
    end;
    curr_val:=result;

    inc (curr_ptr, next);
  end;

end;


function TMainForm.GetNextDiff:integer;
var s:integer;
begin
     result:= GetNextSample;
     if result=-$ffff then exit;
     result:=wave_state.curr_val - wave_state.prev_val;
     if result<0 then result:= -result;
end;

function TMainForm.CalculateVolume: integer;
var
  twst:TWaveState;
  i,val:integer;
  sum: UINT32;
begin
  sum:=0;
  i:=0;
  val:=GetNextSample;
  while val<>-$ffff do
  begin
    sum+=abs(val);
    inc(i);
    if sum>$60000000 then break;
    val:=GetNextSample;
   end;

  result:=sum div i;
end;

procedure TMainForm.AnalyzeLevels;
var i,val:Integer;
  twst:TWaveState;
  strt:integer;
begin
  for i:=0 to 65535 do analyze_buffer[i]:=0;
  twst:=wave_state;
  val:=GetNextDiff;
  while val<>-$ffff do
  begin
    inc(analyze_buffer[val]);
    val:=GetNextDiff;
  end;

  if false then
                      begin

                    i:=65535;
                    while i>0 do begin
                          if analyze_buffer[i]>0 then break;
                          dec(i);
                    end;
                    while i> 0 do
                    begin
                        if analyze_buffer[i]>3500 then begin
                      LogWindow.Append(inttostr(i)+':'+IntToStr(analyze_buffer[i]));
                      Application.ProcessMessages;

                        end;

                      dec(i);
                    end;
  end;
  i:=wave_state.levels-1;
  val:=i;
  while i>4*(1+256*(wave_state.resol-1)) do begin
        if analyze_buffer[val]<=analyze_buffer[i] then val:=i;
        dec(i);
  end;

    LogWindow.Append('Max at:'+IntToStr(val));
               LogWindow.Invalidate;

    wave_state:=twst;
    wave_state.edge_threshold:= val div 2;

end;

function TMainForm.GetImpulseLength:integer;
var l,mx,val:integer;
begin
  val:=GetNextDiff;
  result:=0;
  if val=-$ffff then exit;
  mx:=0;
  l:=0;
  while true do
  begin
      inc(l);
      if mx<val then mx:=val;
      if mx>wave_state.edge_threshold then
         if val<mx then break;
       val:=GetNextDiff;
       if val=-$ffff then exit;

  end;
  result:=l;

end;

function TMainForm.DecodeSector: SStatus;
var i:integer;
    single_sector: array [0..511] of UInt8;
    sectorlength: Integer;
    sectornum: Integer;
    sector_addr: Integer;
        strn:string[200];

    procedure ResetChecksum;
    begin
      chksum:=$57;
    end;

    function CalcChecksum(cntfrom, cntto: integer):UInt8;
             var i:integer;
                 nck: UInt16;
                 carry: UInt16;
    begin
      nck:=chksum;
      result:=chksum;
      for i:= cntfrom downto cntto do

          begin
          nck := nck + single_sector[i];
          carry := (nck and $100) div $100;
          nck:=nck*2 + carry;
          carry := (nck and $100) div $100;
          nck:=nck + carry;
          nck:=nck and $ff;
        end;
      chksum:=nck;
      result :=chksum;
    end;

    procedure DecodeBytes(num:integer);
    var i,bit,byt,val:integer;
    begin
      for i:=num-1 downto 0 do
      begin
          byt:=0;
          bit:=$80;

          while bit>0 do
              begin
                  val:=GetImpulseLength;
                  if val>7 then exit;
                  if val>4 then byt:=byt or bit ;

                    bit:=bit div 2;
              end;

          single_sector[i]:=byt;
      end;
    end;

    function FindHeader: boolean;
    var cnt,val: integer;
    begin
      cnt:=8;
      result:=false;

      while true do begin
          val:=GetImpulseLength;

          if val=0 then exit;
          if (val<=10) and (val>=7) then
            begin
            dec(cnt);
            if cnt=0 then begin
              val:=GetImpulseLength;
               if val<=4 then
                   begin result:= true; exit; end;
               //if val>12 then begin result:=false; exit; end;
               cnt:=8;
            end;
            end
          else
            cnt:=8;
      end;
    end;

begin


  for i:=0 to length(single_sector)-1 do
      single_sector[i]:=0;


  if FindHeader then
  begin
     ResetChecksum;
     DecodeBytes(4);
     CalcChecksum(3,0);
     sectorlength:=single_sector[0]+256*single_sector[1];
     sectornum:=single_sector[2]+256*single_sector[3];

     strn:='Found sector: '+IntToStr(sectornum)+' length: '+inttostr(sectorlength);
     if not ((sectorlength = 128) or (sectorlength=256) or (sectorlength=512)) then exit;
     if sectorlength>xfd_sector_length then xfd_sector_length:=sectorlength;
     if sectornum >=MAX_SECT_NUM then exit;

     if xfd_sector_status[sectornum]<>ssGood then
     begin
     xfd_sector_status[sectornum]:=ssBad;

     DecodeBytes(sectorlength+1);
     CalcChecksum(sectorlength,1);
     if chksum=single_sector[0] then
       begin
         strn+=' CRC OK';
         LogWindow.Append(strn);
         LogWindow.Invalidate;
         //Application.ProcessMessages;

       if sectornum<4 then sector_addr:=128*(sectornum-1) else
            sector_addr:=xfd_sector_length*(sectornum-4)+3*128;
       for i:=1 to sectorlength do begin
             try
             xfd_sector_data[sector_addr+i-1]:=single_sector[i];

             except
               LogWindow.Append('i:'+IntToStr(i));
               LogWindow.Append('a:'+IntToStr(sector_addr));
               LogWindow.Append('s:'+IntToStr(sectornum));
               LogWindow.Append('l:'+IntToStr(xfd_sector_length));

               Application.ProcessMessages;
               sleep(1000);

             end;
       end;
         //LogWindow.Append(IntToStr(sector_addr)+' '+IntToHex(single_sector[0],2));
         //LogWindow.Invalidate;
       xfd_sector_status[sectornum]:=ssGood;
       if MaxSector<sectornum then MaxSector:=sectornum;
       result:=ssGood;
    {   end
     else
       begin
           strn+=' CRC error. Found: '+IntToStr(single_sector[0])+' calculated: '+inttostr(chksum);
           LogWindow.Append(strn);
           Application.ProcessMessages;
           LogWindow.Invalidate;
           Application.ProcessMessages;}
       end;
     end;
     end
  else
  result:=ssEnd;
end;

procedure TMainForm.OutputLengths;
var cnt:integer;
begin
  cnt:=0;
  while cnt<10000 do
  begin
      inc (cnt);
      LogWindow.Append(IntToStr((wave_state.curr_ptr-wave_state.start_ptr)div 2)+':'+IntToStr(GetImpulseLength));
                 LogWindow.Invalidate;

  end;


end;

end.

