provider "kubernetes" {}

resource "kubernetes_namespace" "example" {
  metadata {
    annotations = {
      name = "hetida4office"
    }

    labels = {
      application = "hetida4office"
    }

    name = "hetida4office"
  }
}