Structuri de date utilizate

Primul pas în a realiza acest scop este folosirea unor structuri de date
potrivite pentru operațiile ce au frecvența cea mai mare și duc la modificarea
dinamică a cerințelor de memorie. În acest sens, m-am folosit de implementările
scrise de mine în cadrul laboratoarelor de Structuri de Date și Algoritmi din
anul I, ușor adaptate pentru aplicația de față. Mai exact, pentru ușoara și
rapida căutare a clienților existenți și a topic-urilor la care aceștia sunt
abonați, am ales să folosesc arborii binari de căutare echilibrați în mod
aleator, numiți treap-uri, întrucât implementarea lor este mai ușoară decât
Red-Black sau AVL și au performanțe similare.

Pentru memorarea nesajelor ce urmează să fie trimise clienților, o implementare
de coadă este alegerea ideală; am optat pentru liste dublu înlănțuite cu
santinelă, întrucât sunt ușor de utilizat și îmi oferă posibilitatea de a
șterge un mesaj din această coadă dacă subscriber-ul căruia îi era destinat nu
s-a abonat cu store-and-forward și se deconectează. De asemenea, alocarea
dinamică a fiecărui nod (deci mesaj) din listă duce la evitarea limitării
statice a numărului de mesaje pe care serverul le poate memora la un moment dat.


Protocolul de nivel aplicație

Conform cerinței, a fost necesar să reduc cât de mult posibil overhead-ul
tuturor transmisiilor realizate de cele două componente software, cât și să
folosesc un sistem robust de delimitare a mesajelor individuale. Cerința din
urmă a fost realizată prin inspirarea de la cursul despre nivelul legătura de
date, în care este descrisă metoda încadrării pachetelor folosind caractere de
control. Protocolul aici prezent este o versiune simplificată la maxim,
întrucât utilizează doar 3 caractere de control: DLE, STX și ETX.
Începutul oricărui mesaj constă în caracterele DLE și STX, iar finalul este
întotdeauna DLE și ETX. Pentru cazurile în care datele ce vor fi transmise
conțin caracterul DLE, toate aparițiile acestuia sunt dublate în șirul de
octeți ce va fi trimis prin TCP.

În continuare se descriu cele trei tipuri de mesaje ce se transmit între
subscriber și server:


1. Mesajul de login - conține ID-ul cu care clientul se conectează la server.
Întrcât ID-ul are o lungime maximă destul de redusă, am considerat că a trimite
numărul maxim de octeți necesari pentru a transmite ID-ul cel mai lung acceptat,
eventual terminat cu '\\0' dacă lungimea lui nu este maximă este o metodă simplă
și cu un overhead neglijabil pentru realizarea conectării.


2. Cerere de (un)subscribe - clientul transmite topic-ul la care se
(dez)abonează și, în cazul abonării, parametrul S&F.
întrucât subscribe/unsubscribe (S/U) și S&F sunt valori booleene, au nevoie în
practică doar de un singur bit pentru a fi codificate. Întrucât un octet are
8 biți (din care ne rămân 6 după memorarea datelor antemenționate), putem
folosi cele 64 de valori pe care octetul le mai poate stoca în acei 6 biți
pentru a codifica lungimea topic-ului transmis (căci lungimea acestuia este de
maxim 50 de caractere - suficient de mică pentru a încăpea). Am ales, pentru
ușurința despachetării datelor, să trimit suplimentar și octetul '\\0' după
topic, iar acesta face parte din lungimea memorată în octetul descris anterior,
pe care îl voi numi în continuare "octet de tip" (vezi diagrama de mai jos
pentru o reprezentare vizuală a descrierii octetului de mai sus).

În concluzie, acest mesaj constă în octetul de tip, urmat de numele topic-ului
terminat cu caractetul '\\0'.
   ```
    +----+---+---+---+---+---+---+---+---+
    |Bits| 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 |
    +----+---+---+---+---+---+---+---+---+
    |  0 |S/U|S&F|   Topic length + 1    |
    +----+---+---+---+---+---+---+---+---+
```

3. Mesaj primit de la un client UDP, trimis mai departe de către server unui
subscriber.
Un astfel de mesaj, spre deosebire de cel anterior, are partea de început de
lungime fixată și un format multiplu, în funcție de conținut.
Primii 4 octeți reprezintă adresa IP a clientului UDP ce a trimis mesajul
original, urmat de portul pe care acesta a ajuns la server, ambele memorate în
network byte order. După acestea, urmează un "octet de tip" cu o structură
similară cu octetul de tip de la cererile subscriberilor, cu diferența că
cei mai nesemnificativi 2 biți vor memora acum tipul conținutului, conform
structurilor de transmitere a datelor descrise în enunțul temei.
După acest octet, începe numele topic-ului cu lungime variabilă, pentru care se
aplică aceleași observații de la punctul 2.
În final, urmează conținutul propriu-zis. În ceea ce privește tipurile de date
numerotate de la 0 la 2 (INT, SHORT_REAL și FLOAT), acestea se transmit în
aceeași structură folosită de clienții UDP; tipul de date STRING are, în schimb,
o mică modificare, mai exact, înaintea string-ului propriu-zis se plasează
lungimea acestuia (incluzând octetul '\\0', care este pus mereu la final) sub
forma a doi octeți în network byte order. Antetul acestui mesaj este
reprezentat mai jos:
```
+----+---------------+---------------+---------------+---------------+
|Word|       1       |       2       |       3       |       4       |
+----+---------------+---------------+---------------+---------------+
|Byte|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|2|3|4|5|6|7|8|9|0|1|
+----+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   0|                       Source Address                          |
+----+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   4|         Source Port           |Tip|TopicLen +1|        Topic...
+----+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

Note de final:
 * Ambele programe verifică în cât mai multe moduri atât validitatea datelor
 pe care le primesc, cât și rezultatele diferitelor apeluri de funcții de
 bibliotecă utilizate.
 * Multiplexarea I/O este realizată prin API-ul epoll [1], întrucât l-am
 studiat deja în cadrul materiei Sisteme de Operare din semestrul trecut.
 * În mod similar, o parte din funcțiile și macro-urile de tip "utilitare"
 prezente în această temă sunt inspirate de ultima temă de la materia
 antemenționată [2] și simplificate și/sau augumentate pentru scopul pe care
 acestea îl servesc în cadrul acestui program.
 * Pentru o și mai bună rezistență a acestui software la input invalid/corupt,
 se putea adăuga un checksum la toate datele transmise, pentru a elimina
 aproape complet cazurile în care datele corupte influențează rezultatele
 aplicației; în plus, integritatea datelor primite prin UDP ar fi putut fi
 verificată și de server înainte de a le trimite mai departe la clienți.
 Cu toate acestea, am ales să nu fac aceste lucruri din considerente de viteză,
 căci coruperea datelor nu ar duce la erori critice, iar verificarea acestora
 este oricum imperativă înaintea afișării de către subscriberi.
 * Memorarea topic-urilor la care un subscriber este abonat este realizată
 integral de server, conform cerinței (doar mesajele la care clientul este
 abonat îi vor fi trimise).


Resurse utilizate:
 [1] https://man7.org/linux/man-pages/man7/epoll.7.html
 [2] http://elf.cs.pub.ro/so/res/teme/tema5-util/lin/
 [3] https://pcom.pages.upb.ro/labs/lab5/udp.html
 [4] https://pcom.pages.upb.ro/labs/lab7/tcp.html
 [5] https://man7.org/linux/man-pages/man7/udp.7.html
 [6] https://man7.org/linux/man-pages/man7/tcp.7.html
