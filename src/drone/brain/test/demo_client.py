import json
import urllib.request

def main():
    url = "http://127.0.0.1:7000/plan"

    data = json.dumps({
        "message": "Hello Brain!"
    }).encode()

    request = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"}
    )

    response = urllib.request.urlopen(request)

    print(response.read().decode())

if __name__ == "__main__":
    main()