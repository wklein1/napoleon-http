(function initExamplePage(){

  const goToDocsButton = document.getElementById("goToDocsButton");
  if (goToDocsButton) {
    goToDocsButton.addEventListener("click", () => {
      window.location.href = "/docs/";
    });
  }

const cakeImage = document.getElementById("cakeImage");
const cakePlaceholder = document.getElementById("cakePlaceholder");

function showPlaceholder(){
  if (cakeImage) cakeImage.hidden = true;
  if (cakePlaceholder) cakePlaceholder.hidden = false;
}

function hidePlaceholder(){
  if (cakePlaceholder) cakePlaceholder.hidden = true;
  if (cakeImage) cakeImage.hidden = false;
}

if (cakeImage){
  cakeImage.addEventListener("load", hidePlaceholder, { once: true });
  cakeImage.addEventListener("error", showPlaceholder, { once: true });

  if (cakeImage.complete){
    if (cakeImage.naturalWidth > 0) hidePlaceholder();
    else showPlaceholder();
  }
}

  const yearElem = document.getElementById("currentYear");
  if (yearElem) yearElem.textContent = String(new Date().getFullYear());

})();
