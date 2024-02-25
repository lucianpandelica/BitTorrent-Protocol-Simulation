			
# BitTorrent Protocol

## Pentru client:

Am creat o clasa 'ClientData' pentru a retine date ale clientului, precum:
- 'stored_files' - fisiere pe care clientul le detine la un moment dat
		   (sau segmente) reprezentate prin structura 'StoredFile'
- 'wanted_files' - fisiere pe care clientul le doreste (si statusul
		   descarcarii lor) reprezentate prin structura 'WantedFile'
- 'updates' - update-uri de trimis catre tracker (pentru fiecare fisier retine
	      multimea indicilor segmentelor primite)
- 'requests' - numarul de cereri pe care le-am facut catre un anumit client
- 'init_files_wanted' - numarul de fisiere dorite initial de catre client
- 'files_finished' - numarul de fisiere pentru care am terminat descarcarea
- 'ID' - rangul clientului in MPI_COMM_WORLD

Aceasta contine si metode pentru efectuarea operatiilor descrise de protocolul
BitTorrent, ce vor fi detaliate mai jos.

Vom analiza fiecare thread in parte:

### Thread-ul de download:

Am retinut fisierele detinute de client in vectorul global alocat dinamic
'client_stored', vector de structuri 'StoredFile', ce contin:
- numele fisierului
- numarul de segmente
- hash-urile segmentelor, in ordine

De asemenea, am retinut numele fisierelor dorite de client in vectorul global
alocat dinamic 'tracker_wanted_init', vector de structuri 'TrackerWantedFile',
ce contin numele fisierului.

Pentru trimiterea fisierelor detinute catre tracker, am trimis datele separat,
in ordine, dupa urmatorul protocol ales:
1. numarul de fisiere detinute
2. pentru fiecare fisier: numele, numarul de segmente, hash-urile segm in ordine
3. am asteptat confirmarea primirii de la tracker (un intreg cu valoarea 0)

Pentru trimiterea mesajelor de la client la tracker, am inceput intotdeauna cu
tipul mesajului, unul dintre urmatoarele:
- TYPE_REQ - cerere catre tracker pentru fisierele dorite
- TYPE_UPD - actualizare pentru tracker cu noile segmente detinute
- TYPE_FILE_FIN - finalizare descarcare fisier
- TYPE_CLI_FIN -finalizare descarcare toate fisierele

Vom detalia mai jos protocolul ales pentru fiecare tip de mesaj. In toate cazurile,
se trimite inainte de pasii descrisi un mesaj cu tipul mesajului (un intreg).

** TYPE_REQ:
1. numarul de fisiere dorite
2. pentru fiecare fisier: numele

Drept raspuns, tracker-ul trimite urmatoarele, dupa primirea intregii cereri:
1. pentru fiecare fisier dorit: numele, numar de segmente
2. pentru fiecare segment: hash-ul, numarul de clienti care il detin, clientii
3. din nou la pasul 1

** TYPE_UPD:
1. numarul de fisiere pentru care exista actualizari
2. pentru fiecare fisier: numele, numarul de segmente nou-descarcate, indecsii lor

Drept raspuns, tracker-ul trimite actualizarile sale pentru acel client:
1. numarul de fisiere pentru care exista actualizari
   (dintre cele dorite la mom curent)
2. pentru fiecare fisier: numele, numar de segmente pentru care au aparut clienti
			  noi ca sursa
3. pentru fiecare segment: index, numar noi clienti, rang-urile clientilor

** TYPE_FILE_FIN:
1. numele fisierului

** TYPE_CLI_FIN:

Clientul nu mai trimite nimic. Tracker-ul ii raspunde cu un mesaj de confirmare
(trimite un intreg cu valoarea 0).


Revenind, pentru client, in thread-ul de download, efectuam urmatorii pasi
(dupa ce am transmis datele initiale la tracker):
1. trimitem mesaj de tip TYPE_REQ catre tracker pentru fisierele dorite si asteptam
raspuns
2. initializam structura in care retinem actualizarile
3. cat timp nu am descarcat complet toate fisierele:
	- alegem un segment si un client de la care sa il cerem; daca e cazul,
	  trimitem un mesaj TYPE_FILE_FIN
	- la fiecare 10 segmente trimitem un mesaj TYPE_UPD si resetam continutul
	  structurii ce retine actualizarile
4. trimitem ultimul mesaj TYPE_UPD
5. trimitem mesajul TYPE_CLI_FIN
6. pastram comunicatia cu tracker-ul in asteptarea confirmarii ca toti clientii au
   terminat (un intreg cu valoarea ALL_FINISHED)

Pentru alegerea segmentului si a clientului de la care il cerem, am balansat
incarcarea in urmatorul mod:
- alegerea fisierului pentru care cerem un segment, dintre cele dorite,
  se face aleator
- alegerea segmentului din acest fisier pe care il cerem se face de asemenea
  aleator, din multimea de segmente pe care nu le avem din acest fisier
  (retinem pentru fiecare fisier dorit un set de indici de segmente dorite)
- alegerea clientului de la care cerem segmentul se face pe baza numarului de
  cereri pe care le-am facut la fiecare client ce detine acel segment (folosim
  vectorul 'requests' din clasa ClientData; alegem clientul de la care am
  cerut cele mai putine segmente)

La cererea unui segment, urmam urmatorul protocol:
1. trimitem numele fisierului
2. trimitem index-ul segmentului
3. primim hash-ul segmentului
4. verificam hash-ul primit cu hash-ul pe care il avem de la tracker

### Thread-ul de upload:

Pentru comunicarea intre download_thread si upload_thread (de pe clienti diferiti
sau acelasi client), folosim tag-uri diferite pentru operatii send avand ca sursa
download_thread (UPLOAD_TAG), respectiv operatii send avand ca sursa upload_thread
(DOWNLOAD_TAG) - pentru a evita posibilitatea de a primi in thread-ul de
upload date provenite, spre exemplu, de la tracker (deoarece folosim
MPI_ANY_SOURCE).


## Pentru tracker:

Tracker-ul urmeaza protocolul descris in enunt si detaliat mai sus, in sectiunea
clientului. Dupa ce primeste date de la fiecare client si trimite confirmarea,
asteapta mesaje si, in functie de primul mesaj primit (care specifica tipul de
mesaj si de unde se poate extrage sursa mesajului) efectueaza operatiile necesare
de recv si send - dupa cum s-a descris mai sus.


## Bibliografie:

https://mobylab.docs.crescdi.pub.ro/docs/parallelAndDistributed/laboratory8/
https://www.mpich.org/static/docs/v3.3/www3/MPI_Send.html
https://www.mpich.org/static/docs/v3.3/www3/MPI_Recv.html
https://www.open-mpi.org/doc/v4.1/man3/MPI_Bcast.3.php

