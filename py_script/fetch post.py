from selenium import webdriver
from webdriver_manager.chrome import ChromeDriverManager

main_page = "https://forum.hackinformer.com/viewforum.php?f=115&sid=b4bb9e92260ebc1883c2f54b6a97974a"
browser = webdriver.Chrome(
    ChromeDriverManager().install()
)  # Auto dowwnload\update chrome webdrive


def fetch_post():
    content = browser.find_element_by_class_name("content").text
    game = browser.find_element_by_class_name("topic-title").text

    tmp = (
        f"CUSA \nPS4 {game} \nsource: hackinformer forum \n;Automated file by @Officialahmed0 \n\n:*\n"
        + content
    )

    with open(game + ".savepatch", "w+") as file:
        for line in tmp.split("\n"):
            cmnt = ";" + line
            if "CODE: SELECT ALL" not in line:
                space = line.count(" ")
                if space > 1:
                    file.write(cmnt)
                elif space == 1:
                    if (
                        "0" not in line
                        and "1" not in line
                        and "2" not in line
                        and "3" not in line
                        and "4" not in line
                        and "5" not in line
                        and "6" not in line
                        and "7" not in line
                        and "8" not in line
                        and "9" not in line
                    ):
                        file.write(cmnt)
                    else:
                        file.write(line)
                else:
                    file.write(line + "\n")
                file.write("\n")


# start with page 5 on the forum
topics = (
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    "",
)

for p in topics:
    browser.get(main_page)  # to be changed for var p
    fetch_post()
