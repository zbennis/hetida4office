resource "kubernetes_namespace" "hetida4office" {
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