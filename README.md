# MD5 Checker

### Usage
```
MD5.exe [options [option_args]]

-h | --help
    prints help message

--config config_file
    calculates the hashes of the files in the config and compares them to the provided hashes

--input input_file | input_directory
    calculates the hash of the provided file or recursively all the files in the directory

    ignored if --config is used

--convert input_file
    converts an md5 format config file to a json config file

    ignored if --config or --input is used

--out out_file
    takes the calculated files and outputs them to a config file

    only used with --input or --convert
```

## Config & Formats
**JSON**
If the "Hash" field is left empty the program will output the hash of the file instead of performing a comparison
```json
{
	"Files":  [
		{"Name":  "MD5.exe",  "Hash":  "b489d54b73612e965c96b0c0c834c651"},
		{"Name":  "../filename",  "Hash":  ""},
	]
}
```

**MD5**
This format is not supported directly and must be converted with --convert first
```
b489d54b73612e965c96b0c0c834c651 MD5.exe
79d23d2f860fd04fc464c2890223606c ../example.json
somerandomhashthatislonggoeshere *../filename
```
