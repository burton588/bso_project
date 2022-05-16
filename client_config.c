// AWS IoT client endpoint
const char *client_endpoint = "a2elf1ddh6beu9.iot.us-east-1.amazonaws.com";

// AWS IoT device certificate (ECC)
const char *client_cert =
"-----BEGIN CERTIFICATE-----\r\n"
"MIICfTCCAWWgAwIBAgIUJFohTtyLjiV7aUYstubllm9U2P8wDQYJKoZIhvcNAQEL\r\n"
"BQAwTTFLMEkGA1UECwxCQW1hem9uIFdlYiBTZXJ2aWNlcyBPPUFtYXpvbi5jb20g\r\n"
"SW5jLiBMPVNlYXR0bGUgU1Q9V2FzaGluZ3RvbiBDPVVTMB4XDTIyMDUxNjEyNTUx\r\n"
"NloXDTQ5MTIzMTIzNTk1OVowDTELMAkGA1UEBhMCQVUwWTATBgcqhkjOPQIBBggq\r\n"
"hkjOPQMBBwNCAAR4FtUz2WvoaHQ/enjM7AwBzWar0Qtfi+/jyW8HqqrrPfdlS9VO\r\n"
"RfiTGWMsB5+AH9/9sd9ihWJkWCjZWVWTeqEWo2AwXjAfBgNVHSMEGDAWgBSjpWuW\r\n"
"uw1rSZpsirMKbUmdlKfclzAdBgNVHQ4EFgQUyv3VE0YO5JM7Cgro6JeqpO3WCpQw\r\n"
"DAYDVR0TAQH/BAIwADAOBgNVHQ8BAf8EBAMCB4AwDQYJKoZIhvcNAQELBQADggEB\r\n"
"AGtqRW3KPQrAZN2xXqmwdlwmOffWOp2GRsUXwK8WzfLCDs2r3xdpapxKSd04G7AQ\r\n"
"XD0bNAD3qhHksNOuoV9XdTknsSuOw7qlGCkvjXxzWaSwYm2E3c+dfBUiGqq9NGRs\r\n"
"CisKO9sHOinH4pyeB7ytwb8lp/j8wuhyCebDnDGKXdj277wwUi0ncI/WZrq3uVgQ\r\n"
"6ZE+LgMjDTSNMX7nMK5swZU6XvMrb6RByAahAR2cHlsw6IIjKCiOoewWb+eq1MXe\r\n"
"62TOEFrS6XQ5QnJLTYHWv7i4IAz4S/Qy/4EtWculleNqOuH6IWsidyfUhxwN+3/e\r\n"
"BJbkbfRO1G/TViI7BIlV/78=\r\n"
"-----END CERTIFICATE-----\r\n";


// AWS IoT device private key (ECC)
const char *client_key =
"-----BEGIN EC PARAMETERS-----\r\n"
"BggqhkjOPQMBBw==\r\n"
"-----END EC PARAMETERS-----\r\n"
"-----BEGIN EC PRIVATE KEY-----\r\n"
"MHcCAQEEIJGCyXQKzy820y5IX+LM1VhQXhYZnveGyzwMQRz6j+NOoAoGCCqGSM49\r\n"
"AwEHoUQDQgAEeBbVM9lr6Gh0P3p4zOwMAc1mq9ELX4vv48lvB6qq6z33ZUvVTkX4\r\n"
"kxljLAefgB/f/bHfYoViZFgo2VlVk3qhFg==\r\n"
"-----END EC PRIVATE KEY-----\r\n";
