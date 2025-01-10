# file_synchronization
Final project for the subject Computer networks 2 carried out at the Poznań University of Technology


### Konfiguracja

Najpierw w plikach konfiguracyjnych *"s_config.txt"* oraz *"c_config.txt"* trzeba zmienić ścieżki folderów na swoje (pliki znajdują się w folderach *server* oraz *client*).

**Dla s_config.txt**
Pierwsza linia zawiera ścieżkę do folderu, druga linia zawiera numer portu.

**Dla c_config.txt**
Pierwsza linia zawiera ścieżkę do folderu, druga linia zawiera adres IP serwera, a trzecia linia zawiera numer portu.

```
mkdir build
cd build
cmake ..
make
```
Po tym w folderze *build* pojawią się pliki wykonywalne oraz pliki konfiguracyjne.

### Uruchamianie

**Dla server.cpp**

```
cd build
./server server_config.txt
```

**Dla client.cpp**
```
cd build
./client client_config.txt
```
