AuDecoder, v1.0
---------------

Jak skopiować dyskietkę z Atari 8-bit na platformę PC bez użycia kabli lub przejściówek typu sio2pc, sio2sd, sdrive, sio2usb?

Wystarczy przepisać programik disk2snd.bas do atari uruchomionego bez klawiszy. Możesz uruchomić przedtem DOS i zapisać ten program na dysku do późniejszego wykorzystania. Linie danych są sumowane, więc jeśli popełnisz błąd podczas przepisywania, program Ci to powie. Po 6-7 sekundach od uruchomienia, program poprosi o trzy numery: sektor początkowy (odpowiadasz 1), sektor końcowy (odpowiadasz na przykład 1000, ale 720 będzie działać też, chyba że w ED) i wybrać długość sektora. Pierwsze trzy sektory mają zawsze 128 bajtów, należy wprowadzić regularną długość sektora (dla SD wpisać 1, dla DD wpisać 2). W przypadku błędów, sektory nie będą kodowane na dźwięk. W przypadku pomyłki, zawsze można nacisnąć reset i uruchomić ponownie program (nie trzeba wpisywać go ponownie).

disk2snd jest tak prosty, jak tylko możliwe, więc koduje sektory jeden po drugim - nie jest to najszybsza droga, ale najprostsza i najkrótsza. Odczyt sektorów i kodowanie na dźwięk odbywa się w kodzie maszynowym.

Oczywiście nie musisz przepisywać pierwszych linii z komentarzem :)

Jak tego używać
---------------

Mam nadzieję, że podłączyłeś Atari do PC używając kabelka audio? No dobra, jeden kabelek potrzebny.
I wcisnąłeś nagrywanie w aplikacji audio? (ja polecam Audacity)

Jeśli tak, to uruchom disk2snd i nagraj te zgrzyty i piski do aplikacji audio, zapisz plik jako WAV i załaduj do AuDecoder, który odwróci cały proces do świeżego obrazu dyskietki po stronie PC. Tylko Wavy 8/16 bitów, mono lub stereo są obsługiwane.

Wskazówki:
Dyskietka DD/SS zajmuje ok. 4.5 min dźwięku.
W przypadku błędów przy dekodowaniu można zakodować tylko źle zakodowane sektory i załadować plik ponownie do AuDecoder (pierwszy załąduj duży WAV, potem mały WAV z kilkoma sektorami). Następnie zapisz plik jako ATR lub XFD. Błąd w dekodowaniu może się zdarzyć, kiedy nagrany dźwięk jest niskiej jakości. Pamiętaj, aby nagrywać na 44100Hz! Zarówno dźwięk nagrany w częstotliwości próbkowania 48000 HZ lub niższej niż 44100Hz nie będzie działać. W prawym górnym rogu masz trzy przyciski: pokaż listing disk2snd w basicu, czyszczenie logu oraz resetowanie programu do wartości początkowych.


Słowo na koniec
---------------

Dekoder w ogólności jest bardzo szybki. Dekoduje cały obraz dysku w mniej niż 2 sekundy w większości przypadków. W przypadku poważnych problemów (brak zdekodowanych danych przy użyciu standardowych parametrów) algorytm przechodzi w tryb brute-force - ustawia próg na kolejno zwiększające się wartości aż do zdekodowania wszystkich sektorów lub próg przekroczy maksymalną wartość.
Sygnał nie musi być silny, może stanowić 10% głośności lub więcej, nie przesterowany, nie filtrowany. Przy 16 bitach głośność musi być większa niż 1% :)
Każdy blok danych (szszszszt) zawiera numer sektora, długość sektora i sumę kontrolną, no i oczywiście dane.
Prędkość przesyłu danych to 7800 (same jedynki) do 15600 (tylko zera) bitów na sekundę. Średnio daje to 11 kbitów (brak jest bitów startu i stopu), ok. 1,3 kB/s.

Ponieważ nie mogę dojść do ładu z Lazarusem na moich macbookach (Intel i M1) - przetłumaczyłem main.pas na audecode.c, uruchomiłem i przetesotowałem przy pomocy AI w jeden wieczór:)

Wesołego kopiowania i archiwizowania!

A na koniec najważniejsze: jakkolwiek mozesz używać tego oprogramowania jak chcesz, nie mogę być odpowiedzialny za wszelkie szkody wyrządzone przez to oprogramowanie.
Twój wybór, twoje ryzyko.
Jednak zapewniam, że kod nie zawiera żadnych destrukcyjnych procedur, tylko "zapisz na dysk".

To oprogramowanie jest całkowicie wolne (od "wolności").

Jakub Husak
