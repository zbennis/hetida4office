resource "kubernetes_pod" "hivemq" {
  metadata {
    name = "hetida4office-hivemq"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
      application = "hetida4office"
      service = "hivemq"
    }
  }
  spec {
    container {
      image = "hivemq/hivemq-ce"
      name  = "hivemq-ce"
      port {
        container_port = 1883
      }
    }
  }
}

resource "kubernetes_service" "hivemq" {
  metadata {
    name = "hetida4office-hivemq"
    namespace = kubernetes_namespace.hetida4office.metadata[0].name
    labels = {
        application = "hetida4office"
        service = "hivemq"
    }
  }
  spec {
    selector = {
      service = "hivemq"
    }
    port {
      port        = 1883
      target_port = 1883
    }
    type = "LoadBalancer"
  }
}