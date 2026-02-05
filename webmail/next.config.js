/** @type {import('next').NextConfig} */
const nextConfig = {
  output: 'standalone',
  reactStrictMode: true,
  experimental: {
    serverActions: {
      bodySizeLimit: '10mb',
    },
  },
  // Allow external images if needed
  images: {
    remotePatterns: [],
  },
  // Mark IMAP and nodemailer as external packages
  serverExternalPackages: ['imap', 'mailparser', 'nodemailer'],
}

module.exports = nextConfig
